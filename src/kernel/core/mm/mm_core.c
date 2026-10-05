#include "mm_core.h"
#include "paging.h"
#include "tty.h"
#include "klog.h"

#define MM_MB2_BOOTLOADER_MAGIC 0x36D76289u
#define MM_FLOPPY_MAGIC 0x53314D47u
#define MM_PAGE_SIZE 4096u
#define MM_LOW_MEMORY_LIMIT 0x00100000u
#define MM_MAX_PAGES (0x04000000u / MM_PAGE_SIZE)

typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} mm_mb2_info_t;

typedef struct {
    uint32_t type;
    uint32_t size;
} mm_mb2_tag_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} mm_mb2_mmap_tag_t;

typedef struct {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} mm_mb2_mmap_entry_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
} mm_mb2_module_tag_t;

typedef struct heap_block {
    uint32_t size;
    uint8_t free;
    struct heap_block* next;
    struct heap_block* prev;
} heap_block_t;

extern char __kernel_start[];
extern char __kernel_end[];

static uint8_t g_page_used[MM_MAX_PAGES];
static uint32_t g_total_pages;
static uint32_t g_used_pages;
static uint8_t g_initialized;
static heap_block_t* g_heap_head;
static heap_block_t* g_heap_tail;
static uint32_t g_heap_total_bytes;

static uint32_t mm_align_up_u32(uint32_t value, uint32_t align) {
    return (uint32_t)((value + align - 1u) & ~(align - 1u));
}

static uint32_t mm_align_down_u32(uint32_t value, uint32_t align) {
    return (uint32_t)(value & ~(align - 1u));
}

static uint32_t mm_align_alloc_size(uint32_t size) {
    uint32_t align = (uint32_t)sizeof(uintptr_t);
    return mm_align_up_u32(size, align);
}

static uint32_t mm_mb2_align8(uint32_t value) {
    return (uint32_t)((value + 7u) & ~7u);
}

static void mm_print_hex_u32(uint32_t value) {
    tty_hex_u32(value);
}

static void mm_clear_pages(void) {
    uint32_t i;

    for (i = 0u; i < MM_MAX_PAGES; ++i) {
        g_page_used[i] = 1u;
    }
    g_total_pages = 0u;
    g_used_pages = MM_MAX_PAGES;
}

static void mm_set_page_used(uint32_t page, uint8_t used) {
    if (page >= MM_MAX_PAGES) {
        return;
    }

    if (g_page_used[page] == used) {
        return;
    }

    g_page_used[page] = used;
    if (used != 0u) {
        ++g_used_pages;
    } else {
        --g_used_pages;
    }
}

static void mm_mark_range_free(uint32_t start, uint32_t end) {
    uint32_t addr;
    uint32_t limit = (uint32_t)paging_identity_limit();

    if (start >= limit) {
        return;
    }
    if (end > limit) {
        end = limit;
    }

    start = mm_align_up_u32(start, MM_PAGE_SIZE);
    end = mm_align_down_u32(end, MM_PAGE_SIZE);
    if (start >= end) {
        return;
    }

    for (addr = start; addr < end; addr += MM_PAGE_SIZE) {
        mm_set_page_used(addr / MM_PAGE_SIZE, 0u);
    }
}

static void mm_mark_range_used(uint32_t start, uint32_t end) {
    uint32_t addr;
    uint32_t limit = (uint32_t)paging_identity_limit();

    if (start >= limit) {
        return;
    }
    if (end > limit) {
        end = limit;
    }

    start = mm_align_down_u32(start, MM_PAGE_SIZE);
    end = mm_align_up_u32(end, MM_PAGE_SIZE);
    if (start >= end) {
        return;
    }

    for (addr = start; addr < end; addr += MM_PAGE_SIZE) {
        mm_set_page_used(addr / MM_PAGE_SIZE, 1u);
    }
}

static void mm_count_total_pages(void) {
    uint32_t i;

    g_total_pages = 0u;
    for (i = 0u; i < MM_MAX_PAGES; ++i) {
        if (g_page_used[i] == 0u) {
            ++g_total_pages;
        }
    }
    g_total_pages += g_used_pages;
}

static void mm_mark_mb2_usable(uint32_t boot_info_ptr) {
    const mm_mb2_info_t* info = (const mm_mb2_info_t*)(uintptr_t)boot_info_ptr;
    const uint8_t* end = ((const uint8_t*)info) + info->total_size;
    const mm_mb2_tag_t* tag = (const mm_mb2_tag_t*)(((const uint8_t*)info) + 8u);

    while ((const uint8_t*)tag < end) {
        if (tag->type == 0u) {
            break;
        }
        if (tag->size < sizeof(mm_mb2_tag_t)) {
            break;
        }

        if (tag->type == 6u) {
            const mm_mb2_mmap_tag_t* mmap = (const mm_mb2_mmap_tag_t*)tag;
            const uint8_t* entry_ptr = ((const uint8_t*)mmap) + sizeof(mm_mb2_mmap_tag_t);
            const uint8_t* mmap_end = ((const uint8_t*)mmap) + mmap->size;

            if (mmap->entry_size < sizeof(mm_mb2_mmap_entry_t)) {
                break;
            }

            while (entry_ptr + sizeof(mm_mb2_mmap_entry_t) <= mmap_end) {
                const mm_mb2_mmap_entry_t* entry = (const mm_mb2_mmap_entry_t*)entry_ptr;

                if (entry->type == 1u && entry->addr < 0x100000000ull) {
                    uint64_t end64 = entry->addr + entry->len;
                    if (end64 > 0x100000000ull) {
                        end64 = 0x100000000ull;
                    }
                    mm_mark_range_free((uint32_t)entry->addr, (uint32_t)end64);
                }

                entry_ptr += mmap->entry_size;
            }
        }

        tag = (const mm_mb2_tag_t*)(((const uint8_t*)tag) + mm_mb2_align8(tag->size));
    }
}

static void mm_reserve_mb2_regions(uint32_t boot_info_ptr) {
    const mm_mb2_info_t* info = (const mm_mb2_info_t*)(uintptr_t)boot_info_ptr;
    const uint8_t* end = ((const uint8_t*)info) + info->total_size;
    const mm_mb2_tag_t* tag = (const mm_mb2_tag_t*)(((const uint8_t*)info) + 8u);

    mm_mark_range_used(boot_info_ptr, boot_info_ptr + info->total_size);

    while ((const uint8_t*)tag < end) {
        if (tag->type == 0u) {
            break;
        }
        if (tag->size < sizeof(mm_mb2_tag_t)) {
            break;
        }

        if (tag->type == 3u && tag->size >= sizeof(mm_mb2_module_tag_t)) {
            const mm_mb2_module_tag_t* module = (const mm_mb2_module_tag_t*)tag;
            if (module->mod_end > module->mod_start) {
                mm_mark_range_used(module->mod_start, module->mod_end);
            }
        }

        tag = (const mm_mb2_tag_t*)(((const uint8_t*)tag) + mm_mb2_align8(tag->size));
    }
}

static void mm_reserve_common(uint32_t boot_magic, uint32_t boot_info_ptr) {
    mm_mark_range_used(0u, MM_LOW_MEMORY_LIMIT);
    mm_mark_range_used((uint32_t)(uintptr_t)__kernel_start, (uint32_t)(uintptr_t)__kernel_end);

    if (boot_magic == MM_MB2_BOOTLOADER_MAGIC && boot_info_ptr != 0u) {
        mm_reserve_mb2_regions(boot_info_ptr);
    } else if (boot_magic == MM_FLOPPY_MAGIC && boot_info_ptr != 0u) {
        mm_mark_range_used(boot_info_ptr, boot_info_ptr + MM_PAGE_SIZE);
    }
}

static void* mm_alloc_pages(uint32_t count) {
    uint32_t run = 0u;
    uint32_t start = 0u;
    uint32_t i;
    uint32_t j;

    if (count == 0u) {
        return 0;
    }

    for (i = 0u; i < MM_MAX_PAGES; ++i) {
        if (g_page_used[i] == 0u) {
            if (run == 0u) {
                start = i;
            }
            ++run;
            if (run == count) {
                for (j = 0u; j < count; ++j) {
                    mm_set_page_used(start + j, 1u);
                }
                return (void*)(uintptr_t)(start * MM_PAGE_SIZE);
            }
        } else {
            run = 0u;
        }
    }

    return 0;
}

static void mm_free_pages(void* ptr, uint32_t count) {
    uintptr_t addr = (uintptr_t)ptr;
    uint32_t page;
    uint32_t i;

    if (ptr == 0 || count == 0u || (addr & (MM_PAGE_SIZE - 1u)) != 0u) {
        return;
    }

    page = (uint32_t)(addr / MM_PAGE_SIZE);
    for (i = 0u; i < count && (page + i) < MM_MAX_PAGES; ++i) {
        mm_set_page_used(page + i, 0u);
    }
}

static heap_block_t* mm_find_free_block(uint32_t size) {
    heap_block_t* block = g_heap_head;

    while (block != 0) {
        if (block->free != 0u && block->size >= size) {
            return block;
        }
        block = block->next;
    }

    return 0;
}

static void mm_split_block(heap_block_t* block, uint32_t size) {
    uintptr_t split_addr;
    heap_block_t* split;
    uint32_t min_split = (uint32_t)sizeof(heap_block_t) + (uint32_t)sizeof(uintptr_t);

    if (block == 0 || block->size < size + min_split) {
        return;
    }

    split_addr = (uintptr_t)(block + 1) + size;
    split = (heap_block_t*)split_addr;
    split->size = block->size - size - (uint32_t)sizeof(heap_block_t);
    split->free = 1u;
    split->next = block->next;
    split->prev = block;

    if (split->next != 0) {
        split->next->prev = split;
    } else {
        g_heap_tail = split;
    }

    block->size = size;
    block->next = split;
}

static int mm_blocks_adjacent(heap_block_t* left, heap_block_t* right) {
    uintptr_t left_end;

    if (left == 0 || right == 0) {
        return 0;
    }

    left_end = (uintptr_t)(left + 1) + left->size;
    return (left_end == (uintptr_t)right) ? 1 : 0;
}

static void mm_coalesce_block(heap_block_t* block) {
    if (block == 0) {
        return;
    }

    if (block->next != 0 && block->next->free != 0u && mm_blocks_adjacent(block, block->next) != 0) {
        heap_block_t* next = block->next;
        block->size += (uint32_t)sizeof(heap_block_t) + next->size;
        block->next = next->next;
        if (block->next != 0) {
            block->next->prev = block;
        } else {
            g_heap_tail = block;
        }
    }

    if (block->prev != 0 && block->prev->free != 0u && mm_blocks_adjacent(block->prev, block) != 0) {
        mm_coalesce_block(block->prev);
    }
}

static heap_block_t* mm_grow_heap(uint32_t size) {
    uint32_t total = size + (uint32_t)sizeof(heap_block_t);
    uint32_t pages = (total + MM_PAGE_SIZE - 1u) / MM_PAGE_SIZE;
    heap_block_t* block = (heap_block_t*)mm_alloc_pages(pages);

    if (block == 0) {
        return 0;
    }

    block->size = pages * MM_PAGE_SIZE - (uint32_t)sizeof(heap_block_t);
    block->free = 1u;
    block->next = 0;
    block->prev = g_heap_tail;

    if (g_heap_tail != 0) {
        g_heap_tail->next = block;
    } else {
        g_heap_head = block;
    }
    g_heap_tail = block;
    g_heap_total_bytes += pages * MM_PAGE_SIZE;

    if (block->prev != 0 && block->prev->free != 0u && mm_blocks_adjacent(block->prev, block) != 0) {
        mm_coalesce_block(block->prev);
        return block->prev;
    }

    return block;
}

int mm_core_init(uint32_t boot_magic, uint32_t boot_info_ptr) {
    if (g_initialized != 0u) {
        return 0;
    }

    if (paging_is_enabled() == 0u || paging_identity_limit() < MM_PAGE_SIZE) {
        return -1;
    }

    mm_clear_pages();
    if (boot_magic == MM_MB2_BOOTLOADER_MAGIC && boot_info_ptr != 0u) {
        mm_mark_mb2_usable(boot_info_ptr);
    } else {
        mm_mark_range_free(MM_LOW_MEMORY_LIMIT, (uint32_t)paging_identity_limit());
    }
    mm_reserve_common(boot_magic, boot_info_ptr);
    mm_count_total_pages();

    g_heap_head = 0;
    g_heap_tail = 0;
    g_heap_total_bytes = 0u;
    g_initialized = 1u;
    return 0;
}

void* kmalloc_core(uint32_t size) {
    heap_block_t* block;

    if (g_initialized == 0u || size == 0u) {
        return 0;
    }

    size = mm_align_alloc_size(size);
    block = mm_find_free_block(size);
    if (block == 0) {
        block = mm_grow_heap(size);
        if (block == 0) {
            return 0;
        }
    }

    mm_split_block(block, size);
    block->free = 0u;
    return (void*)(block + 1);
}

void kfree_core(void* ptr) {
    heap_block_t* block;

    if (ptr == 0) {
        return;
    }

    block = ((heap_block_t*)ptr) - 1;
    block->free = 1u;
    mm_coalesce_block(block);
}

void* mm_core_alloc_page(void) {
    if (g_initialized == 0u) {
        return 0;
    }

    return mm_alloc_pages(1u);
}

void mm_core_free_page(void* page) {
    if (g_initialized == 0u) {
        return;
    }

    mm_free_pages(page, 1u);
}

void mm_core_get_stats(mm_stats_t* out) {
    heap_block_t* block;

    if (out == 0) {
        return;
    }

    out->total_pages = g_total_pages;
    out->used_pages = g_used_pages;
    out->free_pages = (g_total_pages >= g_used_pages) ? (g_total_pages - g_used_pages) : 0u;
    out->heap_total_bytes = g_heap_total_bytes;
    out->heap_used_bytes = 0u;
    out->heap_free_bytes = 0u;
    out->heap_largest_free_bytes = 0u;

    block = g_heap_head;
    while (block != 0) {
        if (block->free != 0u) {
            out->heap_free_bytes += block->size;
            if (block->size > out->heap_largest_free_bytes) {
                out->heap_largest_free_bytes = block->size;
            }
        } else {
            out->heap_used_bytes += block->size;
        }
        block = block->next;
    }
}

void mm_core_print_stats(void) {
    mm_stats_t stats;

    mm_core_get_stats(&stats);
    tty_puts("mm pages total=");
    mm_print_hex_u32(stats.total_pages);
    tty_puts(" used=");
    mm_print_hex_u32(stats.used_pages);
    tty_puts(" free=");
    mm_print_hex_u32(stats.free_pages);
    tty_putc('\n');
    tty_puts("mm heap total=");
    mm_print_hex_u32(stats.heap_total_bytes);
    tty_puts(" used=");
    mm_print_hex_u32(stats.heap_used_bytes);
    tty_puts(" free=");
    mm_print_hex_u32(stats.heap_free_bytes);
    tty_puts(" largest=");
    mm_print_hex_u32(stats.heap_largest_free_bytes);
    tty_putc('\n');
}
