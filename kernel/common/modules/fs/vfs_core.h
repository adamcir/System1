#ifndef SYSTEM1_COMMON_VFS_CORE_H
#define SYSTEM1_COMMON_VFS_CORE_H

#include "fs_core.h"

typedef struct {
    int (*init)(void);
    const char* (*get_cwd_path)(void);
    int (*change_dir)(const char* path);
    int (*make_dir)(const char* path);
    int (*list_dir)(const char* path, fs_dirent_t* entries, uint32_t cap, uint32_t* out_count);
    int (*read_file)(const char* path, char* buffer, uint32_t cap, uint32_t* out_size);
    int (*open)(const char* path, uint32_t flags, uint32_t* out_node_id);
    int (*read)(uint32_t node_id, uint32_t offset, char* buffer, uint32_t cap, uint32_t* out_size);
    int (*write)(uint32_t node_id, uint32_t offset, const char* buffer, uint32_t size, uint32_t* out_written);
    int (*size)(uint32_t node_id, uint32_t* out_size);
    int (*close)(uint32_t node_id);
} vfs_driver_t;

#endif
