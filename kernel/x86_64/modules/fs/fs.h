#ifndef SYSTEM1_X64_FS_H
#define SYSTEM1_X64_FS_H

#include "types.h"

#ifndef SYSTEM1_FS_SHARED_DECLS
#define SYSTEM1_FS_SHARED_DECLS

#define FS_NAME_CAP 24u
#define FS_PATH_CAP 128u

#define FS_NODE_DIR 1u
#define FS_NODE_FILE 2u

#define FS_MODE_DIR  0040000u
#define FS_MODE_FILE 0100000u

#define FS_OK 0
#define FS_ERR_NOT_FOUND -1
#define FS_ERR_EXISTS -2
#define FS_ERR_NOT_DIR -3
#define FS_ERR_INVALID -4
#define FS_ERR_NO_SPACE -5
#define FS_ERR_READ_ONLY -6
#define FS_ERR_IS_DIR -7

#define FS_O_RDONLY 0x0001u
#define FS_O_WRONLY 0x0002u
#define FS_O_RDWR   0x0003u
#define FS_O_CREAT  0x0100u
#define FS_O_TRUNC  0x0200u
#define FS_O_APPEND 0x0400u

#define FS_SEEK_SET 0u
#define FS_SEEK_CUR 1u
#define FS_SEEK_END 2u

typedef struct {
    const char* name;
    uint8_t type;
} fs_dirent_t;

typedef struct {
    uint32_t mode;
    uint32_t size;
} fs_stat_t;

#endif

int fs_init(void);
void fs_set_boot_context(uint32_t boot_magic, uint32_t boot_info_ptr);
uint8_t fs_has_pending_changes(void);
int fs_shutdown(uint8_t write_changes);
const char* fs_get_cwd_path(void);
int fs_change_dir(const char* path);
int fs_make_dir(const char* path);
int fs_list_dir(const char* path, fs_dirent_t* entries, uint32_t cap, uint32_t* out_count);
int fs_read_file(const char* path, char* buffer, uint32_t cap, uint32_t* out_size);
int fs_to_errno(int rc);
int fs_stat(const char* path, fs_stat_t* out_stat);
int fs_unlink(const char* path);

#endif
