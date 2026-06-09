#ifndef SYSTEM1_X86_64_MM_H
#define SYSTEM1_X86_64_MM_H

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

int mm_init(uint32_t boot_magic, uint32_t boot_info_ptr);
void* kmalloc(uint32_t size);
void kfree(void* ptr);
void* mm_alloc_page(void);
void mm_free_page(void* page);
void mm_get_stats(mm_stats_t* out);
void mm_print_stats(void);

#endif
