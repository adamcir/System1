#include "fs.h"
#include "fs_core.h"

int fs_init(void) {
    return fs_core_init();
}

const char* fs_get_cwd_path(void) {
    return fs_core_get_cwd_path();
}

int fs_change_dir(const char* path) {
    return fs_core_change_dir(path);
}

int fs_make_dir(const char* path) {
    return fs_core_make_dir(path);
}

int fs_list_dir(const char* path, fs_dirent_t* entries, uint32_t cap, uint32_t* out_count) {
    return fs_core_list_dir(path, entries, cap, out_count);
}
