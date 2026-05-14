#ifndef SYSTEM1_COMMON_VFS_CORE_H
#define SYSTEM1_COMMON_VFS_CORE_H

#include "fs_core.h"

typedef struct {
    int (*init)(void);
    const char* (*get_cwd_path)(void);
    int (*change_dir)(const char* path);
    int (*make_dir)(const char* path);
    int (*list_dir)(const char* path, fs_dirent_t* entries, uint32_t cap, uint32_t* out_count);
} vfs_driver_t;

#endif
