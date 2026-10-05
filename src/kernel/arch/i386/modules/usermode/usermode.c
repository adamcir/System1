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
#define USERMODE_KERNEL_STACK_SIZE 8192u

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
static uint8_t g_usermode_kernel_stack[USERMODE_KERNEL_STACK_SIZE] __attribute__((aligned(16)));

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
    if (rc == SPRG_ERR_NOT_FOUND) {
        return -POSIX_ENOENT;
    }
    if (rc == SPRG_ERR_TOO_LARGE) {
        return -POSIX_ENOSPC;
    }
    if (rc == SPRG_ERR_BAD_MAGIC || rc == SPRG_ERR_BAD_VERSION ||
        rc == SPRG_ERR_BAD_ARCH || rc == SPRG_ERR_BAD_HEADER ||
        rc == SPRG_ERR_UNSUPPORTED) {
        return -POSIX_ENOEXEC;
    }
    return -POSIX_EINVAL;
}

int usermode_init(void) {
    uint32_t tss_base = (uint32_t)(uintptr_t)&g_usermode_tss;

    zero_bytes(g_usermode_gdt, (uint32_t)sizeof(g_usermode_gdt));
    zero_bytes(&g_usermode_tss, (uint32_t)sizeof(g_usermode_tss));

    gdt_set(1u, 0u, 0xFFFFFu, 0x9Au, 0xCFu);
    gdt_set(2u, 0u, 0xFFFFFu, 0x92u, 0xCFu);
    gdt_set(3u, 0u, 0xFFFFFu, 0xFAu, 0xCFu);
    gdt_set(4u, 0u, 0xFFFFFu, 0xF2u, 0xCFu);
    gdt_set(5u, tss_base, (uint32_t)sizeof(g_usermode_tss) - 1u, 0x89u, 0x00u);

    g_usermode_tss.ss0 = GDT_KERNEL_DATA;
    g_usermode_tss.esp0 = (uint32_t)(uintptr_t)
        &g_usermode_kernel_stack[USERMODE_KERNEL_STACK_SIZE];
    g_usermode_tss.iomap_base = (uint16_t)sizeof(g_usermode_tss);

    g_usermode_gdt_ptr.limit = (uint16_t)(sizeof(g_usermode_gdt) - 1u);
    g_usermode_gdt_ptr.base = (uint32_t)(uintptr_t)&g_usermode_gdt[0];

    usermode_i386_load_gdt(&g_usermode_gdt_ptr);
    usermode_i386_load_tss(GDT_TSS);

    if (paging_set_user_range(USERMODE_I386_STACK_MIN,
                              USERMODE_I386_STACK_TOP, 1u) != 0) {
        return -1;
    }

    return 0;
}

int usermode_exec(const char* path) {
    sprg_image_t image;
    process_t* child;
    uint32_t i;
    int rc;
    int status;

    rc = sprg_load_file(path, SPRG_ARCH_I386,
                        USERMODE_I386_PROGRAM_MIN,
                        USERMODE_I386_PROGRAM_MAX,
                        &image);
    if (rc != SPRG_OK) {
        return sprg_error_to_posix(rc);
    }

    for (i = 0u; i < image.segment_count; ++i) {
        const sprg_segment_t* seg = &image.segments[i];
        uintptr_t end = (uintptr_t)seg->vaddr + (uintptr_t)seg->memsz;
        uint8_t writable = (uint8_t)((seg->flags & SPRG_FLAG_W) != 0u);

        if (paging_set_user_range((uintptr_t)seg->vaddr, end, writable) != 0) {
            return -POSIX_EIO;
        }
    }

    child = process_core_spawn_exec(path, image.arch, image.entry,
                                    image.segment_count, image.file_size,
                                    USERMODE_I386_STACK_TOP);
    if (child == 0) {
        return -POSIX_ENOSPC;
    }

    status = usermode_i386_enter(image.entry, USERMODE_I386_STACK_TOP);
    process_core_reap(child);
    return status;
}

void usermode_exit(int status) {
    process_core_exit_current(status);
    usermode_i386_return(status);
    __builtin_unreachable();
}
