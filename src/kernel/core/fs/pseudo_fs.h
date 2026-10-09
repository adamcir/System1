#ifndef SYSTEM_PSEUDO_FS_H
#define SYSTEM_PSEUDO_FS_H
#include "fs_core.h"

#define PSEUDO_FD_TAG 0x80000000u
#define PSEUDO_IS_FD(id) (((id) & PSEUDO_FD_TAG) != 0u)

int pseudo_fs_is_path(const char* path);
int pseudo_fs_change_dir(const char* path);
int pseudo_fs_list_dir(const char* path, fs_dirent_t* entries, uint32_t cap, uint32_t* count);
int pseudo_fs_append_root(fs_dirent_t* entries, uint32_t cap, uint32_t* count);
int pseudo_fs_stat(const char* path, fs_stat_t* stat);
int pseudo_fs_open(const char* path, uint32_t flags, uint32_t* id);
int pseudo_fs_read(uint32_t id, uint32_t offset, char* buf, uint32_t cap, uint32_t* size);
int pseudo_fs_write(uint32_t id, uint32_t offset, const char* buf, uint32_t len, uint32_t* size);
int pseudo_fs_size(uint32_t id, uint32_t* size);
int pseudo_fs_fstat(uint32_t id, fs_stat_t* stat);
int pseudo_fs_close(uint32_t id);
int pseudo_fs_read_file(const char* path, char* buf, uint32_t cap, uint32_t* size);
#endif
