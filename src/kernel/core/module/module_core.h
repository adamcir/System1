#ifndef SYSTEM_MODULE_CORE_H
#define SYSTEM_MODULE_CORE_H
#include "types.h"
#include "smod.h"

int smod_core_boot_load_all(uint32_t boot_magic);
int smod_core_preload(const uint8_t* file, uint32_t bytes, const char* name);

#endif
