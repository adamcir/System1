#include "fs_core.h"
#include "ramfs_core.h"
#include "vfs_core.h"

static const vfs_driver_t g_ramfs_driver = {
    ramfs_core_init,
    ramfs_core_get_cwd_path,
    ramfs_core_change_dir,
    ramfs_core_make_dir,
    ramfs_core_list_dir
};

static const vfs_driver_t* g_root_driver = 0;

int fs_core_init(void) {
    int rc;

    g_root_driver = &g_ramfs_driver;
    rc = g_root_driver->init();
    if (rc != FS_OK) {
        g_root_driver = 0;
        return rc;
    }

    return FS_OK;
}

const char* fs_core_get_cwd_path(void) {
    if (g_root_driver == 0 || g_root_driver->get_cwd_path == 0) {
        return "/";
    }

    return g_root_driver->get_cwd_path();
}

int fs_core_change_dir(const char* path) {
    if (g_root_driver == 0 || g_root_driver->change_dir == 0) {
        return FS_ERR_INVALID;
    }

    return g_root_driver->change_dir(path);
}

int fs_core_make_dir(const char* path) {
    if (g_root_driver == 0 || g_root_driver->make_dir == 0) {
        return FS_ERR_READ_ONLY;
    }

    return g_root_driver->make_dir(path);
}

int fs_core_list_dir(const char* path, fs_dirent_t* entries, uint32_t cap, uint32_t* out_count) {
    if (g_root_driver == 0 || g_root_driver->list_dir == 0) {
        return FS_ERR_INVALID;
    }

    return g_root_driver->list_dir(path, entries, cap, out_count);
}
