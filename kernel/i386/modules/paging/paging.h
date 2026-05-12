#ifndef SYSTEM1_I386_PAGING_H
#define SYSTEM1_I386_PAGING_H

#include "types.h"

int paging_init(uint32_t boot_magic, uint32_t boot_info_ptr);
uint8_t paging_is_enabled(void);
uintptr_t paging_identity_limit(void);
void paging_handle_page_fault(void);

#endif
