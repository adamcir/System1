#ifndef SYSTEM1_COMMON_FAT12_CORE_H
#define SYSTEM1_COMMON_FAT12_CORE_H

#include "block_core.h"
#include "vfs_core.h"

int fat12_core_mount(block_device_t* dev);
const vfs_driver_t* fat12_core_driver(void);

#endif
