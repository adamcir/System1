#include "block_core.h"

static block_device_t* g_root_device = 0;

void block_core_set_root_device(block_device_t* dev) {
    g_root_device = dev;
}

block_device_t* block_core_get_root_device(void) {
    return g_root_device;
}

int block_core_read(block_device_t* dev, uint32_t lba, uint32_t count, void* buffer) {
    if (dev == 0 || dev->read == 0 || buffer == 0 || count == 0u) {
        return FS_ERR_INVALID;
    }

    if (dev->sector_size != 512u) {
        return FS_ERR_INVALID;
    }

    if (dev->sector_count != 0u && (lba >= dev->sector_count || count > (dev->sector_count - lba))) {
        return FS_ERR_INVALID;
    }

    return dev->read(dev, lba, count, buffer);
}

int block_core_write(block_device_t* dev, uint32_t lba, uint32_t count, const void* buffer) {
    if (dev == 0 || dev->write == 0 || buffer == 0 || count == 0u) {
        return FS_ERR_READ_ONLY;
    }

    if (dev->sector_size != 512u) {
        return FS_ERR_INVALID;
    }

    if (dev->sector_count != 0u && (lba >= dev->sector_count || count > (dev->sector_count - lba))) {
        return FS_ERR_INVALID;
    }

    return dev->write(dev, lba, count, buffer);
}
