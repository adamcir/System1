#ifndef SYSTEM1_I386_FLOPPY_FS_H
#define SYSTEM1_I386_FLOPPY_FS_H

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

typedef struct {
    const char* name;
    uint8_t type;
} fs_dirent_t;

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

#endif
