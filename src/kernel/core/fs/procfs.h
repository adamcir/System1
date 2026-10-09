#ifndef SYSTEM_PROCFS_H
#define SYSTEM_PROCFS_H
#include "fs_core.h"

int procfs_read(uint32_t which, uint32_t offset, char* out, uint32_t cap, uint32_t* read);
int procfs_size(uint32_t which, uint32_t* size);
#endif
