#include "paging.h"
#include "paging_core.h"

int paging_init(uint32_t boot_magic, uint32_t boot_info_ptr) {
    return paging_core_init(boot_magic, boot_info_ptr);
}

uint8_t paging_is_enabled(void) {
    return paging_core_is_enabled();
}

uintptr_t paging_identity_limit(void) {
    return paging_core_identity_limit();
}

void paging_handle_page_fault(void) {
    paging_core_handle_page_fault();
}
