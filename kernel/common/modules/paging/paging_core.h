#ifndef SYSTEM1_COMMON_PAGING_CORE_H
#define SYSTEM1_COMMON_PAGING_CORE_H

#include "types.h"

int paging_core_init(uint32_t boot_magic, uint32_t boot_info_ptr);
uint8_t paging_core_is_enabled(void);
uintptr_t paging_core_identity_limit(void);
void paging_core_handle_page_fault(void);

#endif
