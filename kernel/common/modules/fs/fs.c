#include "fs.h"
#include "fs_core.h"

int fs_init(void) {
    return fs_core_init();
}

void fs_set_boot_context(uint32_t boot_magic, uint32_t boot_info_ptr) {
    fs_core_set_boot_context(boot_magic, boot_info_ptr);
}

uint8_t fs_has_pending_changes(void) {
    return fs_core_has_pending_changes();
}

int fs_shutdown(uint8_t write_changes) {
    return fs_core_shutdown(write_changes);
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

int fs_read_file(const char* path, char* buffer, uint32_t cap, uint32_t* out_size) {
    return fs_core_read_file(path, buffer, cap, out_size);
}
