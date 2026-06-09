#ifndef SYSTEM1_COMMON_FS_CORE_H
#define SYSTEM1_COMMON_FS_CORE_H

#include "types.h"

#ifndef SYSTEM1_FS_SHARED_DECLS
#define SYSTEM1_FS_SHARED_DECLS

#define FS_NAME_CAP 24u
#define FS_PATH_CAP 128u

#define FS_NODE_DIR 1u
#define FS_NODE_FILE 2u

#define FS_OK 0
#define FS_ERR_NOT_FOUND -1
#define FS_ERR_EXISTS -2
#define FS_ERR_NOT_DIR -3
#define FS_ERR_INVALID -4
#define FS_ERR_NO_SPACE -5
#define FS_ERR_READ_ONLY -6

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

#endif

int fs_core_init(void);
void fs_core_set_boot_context(uint32_t boot_magic, uint32_t boot_info_ptr);
uint8_t fs_core_has_pending_changes(void);
int fs_core_shutdown(uint8_t write_changes);
const char* fs_core_get_cwd_path(void);
int fs_core_change_dir(const char* path);
int fs_core_make_dir(const char* path);
int fs_core_list_dir(const char* path, fs_dirent_t* entries, uint32_t cap, uint32_t* out_count);
int fs_core_read_file(const char* path, char* buffer, uint32_t cap, uint32_t* out_size);
int fs_core_to_errno(int rc);
int fs_core_normalize_path(const char* cwd, const char* path, char* out, uint32_t out_cap);
int fs_core_open(const char* path, uint32_t flags, uint32_t* out_node_id);
int fs_core_read(uint32_t node_id, uint32_t offset, char* buffer, uint32_t cap, uint32_t* out_size);
int fs_core_write(uint32_t node_id, uint32_t offset, const char* buffer, uint32_t size, uint32_t* out_written);
int fs_core_size(uint32_t node_id, uint32_t* out_size);
int fs_core_close(uint32_t node_id);

#endif
