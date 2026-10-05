#include "usermode.h"
#include "usermode_core.h"

#include "paging.h"
#include "posix_errno.h"
#include "process_core.h"
#include "sprg.h"
#include "types.h"

#define GDT_KERNEL_CODE 0x08u
#define GDT_KERNEL_DATA 0x10u
#define GDT_USER_CODE   0x1Bu
#define GDT_USER_DATA   0x23u
#define GDT_TSS         0x28u

#define USERMODE_KERNEL_STACK_SIZE 4096u
#define USERMODE_EXEC_DEPTH_MAX 2u
#define USERMODE_ARG_MAX 16u
#define USERMODE_ENV_MAX 8u
#define USERMODE_STRING_MAX 128u

typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} gdt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} gdt_ptr_t;

typedef struct __attribute__((packed)) {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} tss32_t;

extern void usermode_i386_load_gdt(const gdt_ptr_t* ptr);
extern void usermode_i386_load_tss(uint32_t selector);
extern int usermode_i386_enter(uint32_t entry, uint32_t user_stack);
extern void usermode_i386_return(int status) __attribute__((noreturn));

uint32_t g_usermode_saved_esp;
uint32_t g_usermode_saved_eflags;

static gdt_entry_t g_usermode_gdt[6];
static gdt_ptr_t g_usermode_gdt_ptr;
static tss32_t g_usermode_tss;
static uint8_t g_usermode_kernel_stacks[USERMODE_EXEC_DEPTH_MAX][USERMODE_KERNEL_STACK_SIZE]
    __attribute__((aligned(16)));
static uint32_t g_usermode_exec_depth;

static void zero_bytes(void* ptr, uint32_t size) {
    uint8_t* out = (uint8_t*)ptr;
    uint32_t i;
    for (i = 0u; i < size; ++i) {
        out[i] = 0u;
    }
}

static void gdt_set(uint32_t index, uint32_t base, uint32_t limit,
                    uint8_t access, uint8_t granularity) {
    gdt_entry_t* entry = &g_usermode_gdt[index];
    entry->base_low = (uint16_t)(base & 0xFFFFu);
    entry->base_mid = (uint8_t)((base >> 16) & 0xFFu);
    entry->base_high = (uint8_t)((base >> 24) & 0xFFu);
    entry->limit_low = (uint16_t)(limit & 0xFFFFu);
    entry->granularity = (uint8_t)(((limit >> 16) & 0x0Fu) | (granularity & 0xF0u));
    entry->access = access;
}

static int sprg_error_to_posix(int rc) {
    if (rc == SPRG_ERR_NOT_FOUND) return -POSIX_ENOENT;
    if (rc == SPRG_ERR_TOO_LARGE) return -POSIX_ENOSPC;
    if (rc == SPRG_ERR_BAD_MAGIC || rc == SPRG_ERR_BAD_VERSION ||
        rc == SPRG_ERR_BAD_ARCH || rc == SPRG_ERR_BAD_HEADER ||
        rc == SPRG_ERR_UNSUPPORTED) {
        return -POSIX_ENOEXEC;
    }
    return -POSIX_EINVAL;
}

static int image_fits_slot(const sprg_image_t* image, uintptr_t min, uintptr_t max) {
    uint32_t i;
    for (i = 0u; i < image->segment_count; ++i) {
        uintptr_t start = (uintptr_t)image->segments[i].vaddr;
        uintptr_t end = start + (uintptr_t)image->segments[i].memsz;
        if (end < start || start < min || end > max) {
            return 0;
        }
    }
    return 1;
}

static int copy_stack_string(uintptr_t* sp, uintptr_t stack_min,
                             const char* text, uintptr_t* out_addr) {
    uint32_t len = 0u;
    uint32_t i;
    uintptr_t next;

    if (text == 0) return -POSIX_EINVAL;
    while (text[len] != '\0') {
        if (len + 1u >= USERMODE_STRING_MAX) return -POSIX_E2BIG;
        ++len;
    }
    ++len;

    if (*sp < stack_min + len) return -POSIX_ENOSPC;
    next = *sp - len;
    for (i = 0u; i < len; ++i) {
        ((char*)next)[i] = text[i];
    }

    *sp = next;
    *out_addr = next;
    return 0;
}

static int build_initial_stack(uintptr_t stack_min, uintptr_t stack_top,
                               const char* path,
                               char* const argv[], char* const envp[],
                               uintptr_t* out_sp) {
    uintptr_t sp = stack_top;
    uintptr_t argv_addr[USERMODE_ARG_MAX];
    uintptr_t env_addr[USERMODE_ENV_MAX];
    uint32_t argc = 0u;
    uint32_t envc = 0u;
    uint32_t i;
    int rc;

    if (argv != 0) {
        while (argv[argc] != 0) {
            if (argc >= USERMODE_ARG_MAX) return -POSIX_E2BIG;
            ++argc;
        }
    }
    if (argc == 0u) {
        argc = 1u;
    }

    if (envp != 0) {
        while (envp[envc] != 0) {
            if (envc >= USERMODE_ENV_MAX) return -POSIX_E2BIG;
            ++envc;
        }
    }

    for (i = 0u; i < envc; ++i) {
        rc = copy_stack_string(&sp, stack_min, envp[i], &env_addr[i]);
        if (rc < 0) return rc;
    }

    for (i = 0u; i < argc; ++i) {
        const char* arg = (argv != 0 && argv[0] != 0) ? argv[i] : path;
        rc = copy_stack_string(&sp, stack_min, arg, &argv_addr[i]);
        if (rc < 0) return rc;
    }

    sp &= ~(uintptr_t)3u;

#define PUSH_U32(v) do {     if (sp < stack_min + 4u) return -POSIX_ENOSPC;     sp -= 4u;     *(uint32_t*)sp = (uint32_t)(v); } while (0)

    PUSH_U32(0u);
    for (i = envc; i > 0u; --i) PUSH_U32(env_addr[i - 1u]);
    PUSH_U32(0u);
    for (i = argc; i > 0u; --i) PUSH_U32(argv_addr[i - 1u]);
    PUSH_U32(argc);

#undef PUSH_U32

    *out_sp = sp;
    return 0;
}

int usermode_init(void) {
    uint32_t tss_base = (uint32_t)(uintptr_t)&g_usermode_tss;

    zero_bytes(g_usermode_gdt, (uint32_t)sizeof(g_usermode_gdt));
    zero_bytes(&g_usermode_tss, (uint32_t)sizeof(g_usermode_tss));
    zero_bytes(g_usermode_kernel_stacks, (uint32_t)sizeof(g_usermode_kernel_stacks));

    gdt_set(1u, 0u, 0xFFFFFu, 0x9Au, 0xCFu);
    gdt_set(2u, 0u, 0xFFFFFu, 0x92u, 0xCFu);
    gdt_set(3u, 0u, 0xFFFFFu, 0xFAu, 0xCFu);
    gdt_set(4u, 0u, 0xFFFFFu, 0xF2u, 0xCFu);
    gdt_set(5u, tss_base, (uint32_t)sizeof(g_usermode_tss) - 1u, 0x89u, 0x00u);

    g_usermode_tss.ss0 = GDT_KERNEL_DATA;
    g_usermode_tss.esp0 = (uint32_t)(uintptr_t)
        &g_usermode_kernel_stacks[0][USERMODE_KERNEL_STACK_SIZE];
    g_usermode_tss.iomap_base = (uint16_t)sizeof(g_usermode_tss);

    g_usermode_gdt_ptr.limit = (uint16_t)(sizeof(g_usermode_gdt) - 1u);
    g_usermode_gdt_ptr.base = (uint32_t)(uintptr_t)&g_usermode_gdt[0];

    usermode_i386_load_gdt(&g_usermode_gdt_ptr);
    usermode_i386_load_tss(GDT_TSS);

    if (paging_set_user_range(USERMODE_I386_STACK_MIN,
                              USERMODE_I386_STACK_TOP, 1u) != 0 ||
        paging_set_user_range(USERMODE_I386_SHELL_STACK_MIN,
                              USERMODE_I386_SHELL_STACK_TOP, 1u) != 0) {
        return -1;
    }

    g_usermode_exec_depth = 0u;
    return 0;
}

int usermode_exec(const char* path, char* const argv[], char* const envp[]) {
    sprg_image_t image;
    process_t* child;
    uintptr_t program_min;
    uintptr_t program_max;
    uintptr_t stack_min;
    uintptr_t stack_top;
    uintptr_t initial_sp;
    uint32_t old_saved_esp;
    uint32_t old_saved_eflags;
    uint32_t old_tss_esp0;
    uint32_t i;
    int rc;
    int status;

    if (g_usermode_exec_depth >= USERMODE_EXEC_DEPTH_MAX) {
        return -POSIX_ENOSPC;
    }

    rc = sprg_validate_file(path, SPRG_ARCH_I386, &image);
    if (rc != SPRG_OK) return sprg_error_to_posix(rc);

    if (image_fits_slot(&image, USERMODE_I386_PROGRAM_MIN,
                        USERMODE_I386_PROGRAM_MAX)) {
        program_min = USERMODE_I386_PROGRAM_MIN;
        program_max = USERMODE_I386_PROGRAM_MAX;
        stack_min = USERMODE_I386_STACK_MIN;
        stack_top = USERMODE_I386_STACK_TOP;
    } else if (image_fits_slot(&image, USERMODE_I386_SHELL_PROGRAM_MIN,
                               USERMODE_I386_SHELL_PROGRAM_MAX)) {
        if (g_usermode_exec_depth != 0u) {
            return -POSIX_ENOSPC;
        }
        program_min = USERMODE_I386_SHELL_PROGRAM_MIN;
        program_max = USERMODE_I386_SHELL_PROGRAM_MAX;
        stack_min = USERMODE_I386_SHELL_STACK_MIN;
        stack_top = USERMODE_I386_SHELL_STACK_TOP;
    } else {
        return -POSIX_ENOEXEC;
    }

    rc = sprg_load_file(path, SPRG_ARCH_I386, program_min, program_max, &image);
    if (rc != SPRG_OK) return sprg_error_to_posix(rc);

    for (i = 0u; i < image.segment_count; ++i) {
        const sprg_segment_t* seg = &image.segments[i];
        uintptr_t end = (uintptr_t)seg->vaddr + (uintptr_t)seg->memsz;
        uint8_t writable = (uint8_t)((seg->flags & SPRG_FLAG_W) != 0u);
        if (paging_set_user_range((uintptr_t)seg->vaddr, end, writable) != 0) {
            return -POSIX_EIO;
        }
    }

    rc = build_initial_stack(stack_min, stack_top, path, argv, envp, &initial_sp);
    if (rc < 0) return rc;

    child = process_core_spawn_exec(path, image.arch, image.entry,
                                    image.segment_count, image.file_size,
                                    initial_sp);
    if (child == 0) return -POSIX_ENOSPC;

    old_saved_esp = g_usermode_saved_esp;
    old_saved_eflags = g_usermode_saved_eflags;
    old_tss_esp0 = g_usermode_tss.esp0;

    g_usermode_tss.esp0 = (uint32_t)(uintptr_t)
        &g_usermode_kernel_stacks[g_usermode_exec_depth][USERMODE_KERNEL_STACK_SIZE];
    ++g_usermode_exec_depth;

    status = usermode_i386_enter(image.entry, (uint32_t)initial_sp);

    --g_usermode_exec_depth;
    g_usermode_tss.esp0 = old_tss_esp0;
    g_usermode_saved_esp = old_saved_esp;
    g_usermode_saved_eflags = old_saved_eflags;

    process_core_reap(child);
    return status;
}

void usermode_exit(int status) {
    process_core_exit_current(status);
    usermode_i386_return(status);
    __builtin_unreachable();
}
