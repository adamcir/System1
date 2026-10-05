#ifndef SYSTEM1_COMMON_MM_CORE_H
#define SYSTEM1_COMMON_MM_CORE_H

#include "types.h"

#ifndef SYSTEM1_MM_SHARED_DECLS
#define SYSTEM1_MM_SHARED_DECLS

typedef struct {
    uint32_t total_pages;
    uint32_t used_pages;
    uint32_t free_pages;
    uint32_t heap_total_bytes;
    uint32_t heap_used_bytes;
    uint32_t heap_free_bytes;
    uint32_t heap_largest_free_bytes;
} mm_stats_t;

#endif

int mm_core_init(uint32_t boot_magic, uint32_t boot_info_ptr);
void* kmalloc_core(uint32_t size);
void kfree_core(void* ptr);
void* mm_core_alloc_page(void);
void mm_core_free_page(void* page);
void mm_core_get_stats(mm_stats_t* out);
void mm_core_print_stats(void);

#endif
