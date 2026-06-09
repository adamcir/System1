#include "mm.h"
#include "mm_core.h"

int mm_init(uint32_t boot_magic, uint32_t boot_info_ptr) {
    return mm_core_init(boot_magic, boot_info_ptr);
}

void* kmalloc(uint32_t size) {
    return kmalloc_core(size);
}

void kfree(void* ptr) {
    kfree_core(ptr);
}

void* mm_alloc_page(void) {
    return mm_core_alloc_page();
}

void mm_free_page(void* page) {
    mm_core_free_page(page);
}

void mm_get_stats(mm_stats_t* out) {
    mm_core_get_stats(out);
}

void mm_print_stats(void) {
    mm_core_print_stats();
}
