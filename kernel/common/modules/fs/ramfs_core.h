#ifndef SYSTEM1_COMMON_RAMFS_CORE_H
#define SYSTEM1_COMMON_RAMFS_CORE_H

#include "fs_core.h"

int ramfs_core_init(void);
const char* ramfs_core_get_cwd_path(void);
int ramfs_core_change_dir(const char* path);
int ramfs_core_make_dir(const char* path);
int ramfs_core_list_dir(const char* path, fs_dirent_t* entries, uint32_t cap, uint32_t* out_count);

#endif
