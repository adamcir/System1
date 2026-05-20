#ifndef SYSTEM1_COMMON_ISO9660_CORE_H
#define SYSTEM1_COMMON_ISO9660_CORE_H

#include "block_core.h"
#include "vfs_core.h"

int iso9660_core_mount(block_device_t* dev);
const vfs_driver_t* iso9660_core_driver(void);

#endif
