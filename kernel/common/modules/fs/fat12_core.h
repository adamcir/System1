#ifndef SYSTEM1_COMMON_FAT12_CORE_H
#define SYSTEM1_COMMON_FAT12_CORE_H

#include "block_core.h"
#include "vfs_core.h"

int fat12_core_mount(block_device_t* dev);
int fat12_core_create_dir_in_image(uint8_t* image, uint32_t image_size, const char* path);
int fat12_core_write_file_in_image(uint8_t* image, uint32_t image_size, const char* path, const char* data, uint32_t size);
int fat12_core_create_dir_on_device(block_device_t* dev, const char* path);
int fat12_core_write_file_on_device(block_device_t* dev, const char* path, const char* data, uint32_t size);
uint32_t fat12_core_buffer_bytes(void);
const vfs_driver_t* fat12_core_driver(void);

#endif
