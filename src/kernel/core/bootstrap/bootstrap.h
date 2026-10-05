#ifndef SYSTEM1_COMMON_BOOTSTRAP_H
#define SYSTEM1_COMMON_BOOTSTRAP_H

#include "types.h"

int bootstrap_init(uint32_t boot_magic, uint32_t boot_info_ptr);
int bootstrap_start_shell(void);

#endif
