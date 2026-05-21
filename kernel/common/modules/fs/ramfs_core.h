#ifndef SYSTEM1_COMMON_RAMFS_CORE_H
#define SYSTEM1_COMMON_RAMFS_CORE_H

#include "fs_core.h"
#include "vfs_core.h"

int ramfs_core_init(void);
int ramfs_core_reset_empty(void);
int ramfs_core_import_dir(const char* path);
int ramfs_core_import_file(const char* path);
uint8_t ramfs_core_is_dirty(void);
void ramfs_core_clear_dirty(void);
const vfs_driver_t* ramfs_core_driver(void);
const char* ramfs_core_get_cwd_path(void);
int ramfs_core_change_dir(const char* path);
int ramfs_core_make_dir(const char* path);
int ramfs_core_list_dir(const char* path, fs_dirent_t* entries, uint32_t cap, uint32_t* out_count);
int ramfs_core_read_file(const char* path, char* buffer, uint32_t cap, uint32_t* out_size);

#endif
