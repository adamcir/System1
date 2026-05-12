#include "paging_core.h"
#include "klog.h"
#include "vga.h"

#define MB2_BOOTLOADER_MAGIC 0x36D76289u
#define FLOPPY_MAGIC 0x53314D47u

#define PAGE_SIZE 4096u
#define PAGING_IDENTITY_LIMIT 0x04000000u

typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} mb2_info_t;

typedef struct {
    uint32_t type;
    uint32_t size;
} mb2_tag_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} mb2_mmap_tag_t;

typedef struct {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} mb2_mmap_entry_t;

static uint8_t g_paging_enabled = 0;

#if defined(__x86_64__)
static uint64_t g_pml4[512] __attribute__((aligned(PAGE_SIZE)));
static uint64_t g_pdpt[512] __attribute__((aligned(PAGE_SIZE)));
static uint64_t g_pd[512] __attribute__((aligned(PAGE_SIZE)));
static uint64_t g_pt[32][512] __attribute__((aligned(PAGE_SIZE)));
#else
static uint32_t g_pd[1024] __attribute__((aligned(PAGE_SIZE)));
static uint32_t g_pt[16][1024] __attribute__((aligned(PAGE_SIZE)));
#endif

static uint32_t align8(uint32_t value) {
    return (uint32_t)((value + 7u) & ~7u);
}

static uintptr_t read_cr2_addr(void) {
    uintptr_t value;
#if defined(__x86_64__)
    __asm__ volatile ("mov %%cr2, %0" : "=r"(value));
#else
    uint32_t tmp;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(tmp));
    value = (uintptr_t)tmp;
#endif
    return value;
}

static void load_cr3_ptr(uintptr_t value) {
#if defined(__x86_64__)
    __asm__ volatile ("mov %0, %%cr3" : : "r"(value) : "memory");
#else
    __asm__ volatile ("mov %0, %%cr3" : : "r"((uint32_t)value) : "memory");
#endif
}

static void enable_paging_bit(void) {
#if defined(__x86_64__)
    uintptr_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (uintptr_t)(1ull << 31);
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
#else
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (uint32_t)(1u << 31);
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
#endif
}

static int mb2_has_usable_region(uint32_t boot_info_ptr) {
    const mb2_info_t* info;
    const uint8_t* end;
    const mb2_tag_t* tag;

    if (boot_info_ptr == 0u) {
        return 0;
    }

    info = (const mb2_info_t*)(uintptr_t)boot_info_ptr;
    end = ((const uint8_t*)info) + info->total_size;
    tag = (const mb2_tag_t*)(((const uint8_t*)info) + 8u);

    while ((const uint8_t*)tag < end) {
        if (tag->type == 0u) {
            break;
        }
        if (tag->size < sizeof(mb2_tag_t)) {
            break;
        }

        if (tag->type == 6u) {
            const mb2_mmap_tag_t* mmap = (const mb2_mmap_tag_t*)tag;
            const uint8_t* entry_ptr = ((const uint8_t*)mmap) + sizeof(mb2_mmap_tag_t);
            const uint8_t* mmap_end = ((const uint8_t*)mmap) + mmap->size;
            if (mmap->entry_size < sizeof(mb2_mmap_entry_t)) {
                return 0;
            }

            while (entry_ptr + sizeof(mb2_mmap_entry_t) <= mmap_end) {
                const mb2_mmap_entry_t* entry = (const mb2_mmap_entry_t*)entry_ptr;
                if (entry->type == 1u && entry->len != 0u) {
                    return 1;
                }
                entry_ptr += mmap->entry_size;
            }
        }

        tag = (const mb2_tag_t*)(((const uint8_t*)tag) + align8(tag->size));
    }

    return 0;
}

#if defined(__x86_64__)
static void build_identity_tables_x64(void) {
    uint32_t i;
    uint64_t phys = 0u;

    for (i = 0; i < 512u; ++i) {
        g_pml4[i] = 0u;
        g_pdpt[i] = 0u;
        g_pd[i] = 0u;
    }

    for (i = 0; i < 32u; ++i) {
        uint32_t j;
        for (j = 0; j < 512u; ++j) {
            g_pt[i][j] = (phys & 0x000FFFFFFFFFF000ull) | 0x3ull;
            phys += PAGE_SIZE;
        }
    }

    for (i = 0; i < 32u; ++i) {
        g_pd[i] = (((uint64_t)(uintptr_t)&g_pt[i][0]) & 0x000FFFFFFFFFF000ull) | 0x3ull;
    }

    g_pdpt[0] = (((uint64_t)(uintptr_t)&g_pd[0]) & 0x000FFFFFFFFFF000ull) | 0x3ull;
    g_pml4[0] = (((uint64_t)(uintptr_t)&g_pdpt[0]) & 0x000FFFFFFFFFF000ull) | 0x3ull;
}
#else
static void build_identity_tables_x86(void) {
    uint32_t i;
    uint32_t phys = 0u;

    for (i = 0; i < 1024u; ++i) {
        g_pd[i] = 0u;
    }

    for (i = 0; i < 16u; ++i) {
        uint32_t j;
        for (j = 0; j < 1024u; ++j) {
            g_pt[i][j] = (phys & 0xFFFFF000u) | 0x3u;
            phys += PAGE_SIZE;
        }
        g_pd[i] = (((uint32_t)(uintptr_t)&g_pt[i][0]) & 0xFFFFF000u) | 0x3u;
    }
}
#endif

int paging_core_init(uint32_t boot_magic, uint32_t boot_info_ptr) {
    if (g_paging_enabled != 0u) {
        return 0;
    }

    if (boot_magic == MB2_BOOTLOADER_MAGIC) {
        if (mb2_has_usable_region(boot_info_ptr) == 0) {
            klog_info("paging", "mb2 mmap missing usable region");
            return -1;
        }
        klog_info("paging", "init path: multiboot2");
    } else if (boot_magic == FLOPPY_MAGIC) {
        klog_info("paging", "init path: floppy fallback");
    } else {
        klog_info("paging", "init path: generic fallback");
    }

#if defined(__x86_64__)
    build_identity_tables_x64();
    load_cr3_ptr((uintptr_t)&g_pml4[0]);
#else
    build_identity_tables_x86();
    load_cr3_ptr((uintptr_t)&g_pd[0]);
#endif
    enable_paging_bit();

    g_paging_enabled = 1u;
    klog_info("paging", "enabled (identity 64 MiB, 4 KiB pages)");
    return 0;
}

uint8_t paging_core_is_enabled(void) {
    return g_paging_enabled;
}

uintptr_t paging_core_identity_limit(void) {
    return (uintptr_t)PAGING_IDENTITY_LIMIT;
}

void paging_core_handle_page_fault(void) {
    uintptr_t fault_addr = read_cr2_addr();

    vga_set_color(RED);
    vga_puts("[PAGE FAULT] addr=");
    vga_hex_u32((uint32_t)fault_addr);
    vga_putc('\n');
    panic("Unhandled page fault");
}
