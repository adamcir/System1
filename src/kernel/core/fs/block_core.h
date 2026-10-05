#ifndef SYSTEM1_COMMON_BLOCK_CORE_H
#define SYSTEM1_COMMON_BLOCK_CORE_H

#include "types.h"
#include "fs_core.h"

typedef struct block_device block_device_t;

struct block_device {
    uint32_t sector_size;
    uint32_t sector_count;
    void* ctx;
    int (*read)(block_device_t* dev, uint32_t lba, uint32_t count, void* buffer);
    int (*write)(block_device_t* dev, uint32_t lba, uint32_t count, const void* buffer);
};

void block_core_set_root_device(block_device_t* dev);
block_device_t* block_core_get_root_device(void);
int block_core_read(block_device_t* dev, uint32_t lba, uint32_t count, void* buffer);
int block_core_write(block_device_t* dev, uint32_t lba, uint32_t count, const void* buffer);

#endif
