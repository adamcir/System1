#ifndef SYSTEM_DEVFS_H
#define SYSTEM_DEVFS_H
#include "fs_core.h"

int devfs_read(uint32_t which, uint32_t offset, char* out, uint32_t cap, uint32_t* read);
int devfs_write(uint32_t which, const char* input, uint32_t len, uint32_t* written);
#endif
