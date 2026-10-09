#include "pseudo_fs.h"
#include "devfs.h"
#include "procfs.h"

/* Virtual mount points independent of FAT12/ISO9660/RAMFS.
 * File IDs have the top bit set and never alias physical filesystem IDs.
 */
static const char* const dev_names[] = {"null", "zero", "tty", "console", "ttyS0", "kmsg"};
static const char* const proc_names[] = {"version", "uptime", "meminfo", "modules"};
static const char dev_dir[] = "dev", proc_dir[] = "proc";

static int same(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == 0 && *b == 0;
}
static int mount(const char* p) {
    if (!p || p[0] != '/') return 0;
    if (p[1] == 'd' && p[2] == 'e' && p[3] == 'v' &&
        (p[4] == 0 || p[4] == '/')) return 1;
    if (p[1] == 'p' && p[2] == 'r' && p[3] == 'o' && p[4] == 'c' &&
        (p[5] == 0 || p[5] == '/')) return 2;
    return 0;
}
int pseudo_fs_is_path(const char* path) { return mount(path) != 0; }
static int lookup(const char* path) {
    uint32_t i;
    int kind = mount(path);
    if (kind == 1) {
        if (same(path, "/dev")) return -2;
        for (i = 0u; i < 6u; ++i) {
            const char* prefix = path + 5u;
            if (same(prefix, dev_names[i])) return (int)i;
        }
    } else if (kind == 2) {
        if (same(path, "/proc")) return -3;
        for (i = 0u; i < 4u; ++i) {
            const char* prefix = path + 6u;
            if (same(prefix, proc_names[i])) return (int)(i + 6u);
        }
    }
    return -1;
}
int pseudo_fs_change_dir(const char* path) {
    int n = lookup(path);
    if (n == -2 || n == -3) return FS_OK;
    return n == -1 ? FS_ERR_NOT_FOUND : FS_ERR_NOT_DIR;
}
int pseudo_fs_list_dir(const char* path, fs_dirent_t* entries, uint32_t cap, uint32_t* count) {
    int id = lookup(path);
    const char* const* names;
    uint32_t i;
    if (!entries || !count) return FS_ERR_INVALID;
    if (id != -2 && id != -3) return id < 0 ? FS_ERR_NOT_FOUND : FS_ERR_NOT_DIR;
    if (cap < (id == -2 ? 8u : 6u)) return FS_ERR_NO_SPACE;
    names = id == -2 ? dev_names : proc_names;
    entries[0].name = "."; entries[0].type = FS_NODE_DIR;
    entries[1].name = ".."; entries[1].type = FS_NODE_DIR;
    for (i = 0u; i < (id == -2 ? 6u : 4u); ++i) {
        entries[i + 2u].name = names[i];
        entries[i + 2u].type = FS_NODE_FILE;
    }
    *count = (id == -2 ? 8u : 6u);
    return FS_OK;
}
int pseudo_fs_append_root(fs_dirent_t* entries, uint32_t cap, uint32_t* count) {
    uint32_t i;
    int has_dev = 0, has_proc = 0;
    if (!entries || !count || *count > cap) return FS_ERR_INVALID;
    for (i = 0u; i < *count; ++i) {
        if (same(entries[i].name, "dev")) has_dev = 1;
        if (same(entries[i].name, "proc")) has_proc = 1;
    }
    if (*count + (uint32_t)(!has_dev) + (uint32_t)(!has_proc) > cap) return FS_ERR_NO_SPACE;
    if (!has_dev) { entries[*count].name = dev_dir; entries[*count].type = FS_NODE_DIR; ++*count; }
    if (!has_proc) { entries[*count].name = proc_dir; entries[*count].type = FS_NODE_DIR; ++*count; }
    return FS_OK;
}
static int stat_id(int id, fs_stat_t* st) {
    if (!st) return FS_ERR_INVALID;
    if (id == -2 || id == -3) { st->mode = FS_MODE_DIR; st->size = 0u; return FS_OK; }
    if (id < 0 || id >= 10) return FS_ERR_NOT_FOUND;
    st->mode = id < 6 ? FS_MODE_CHAR : FS_MODE_FILE;
    st->size = 0u;
    return id < 6 ? FS_OK : procfs_size((uint32_t)(id - 6), &st->size);
}
int pseudo_fs_stat(const char* path, fs_stat_t* st) { return stat_id(lookup(path), st); }
int pseudo_fs_open(const char* path, uint32_t flags, uint32_t* out) {
    int id = lookup(path);
    if (!out) return FS_ERR_INVALID;
    if (id == -2 || id == -3) return FS_ERR_IS_DIR;
    if (id < 0) return FS_ERR_NOT_FOUND;
    if (flags & (FS_O_CREAT | FS_O_TRUNC)) return FS_ERR_READ_ONLY;
    if (id >= 6 && (flags & FS_O_WRONLY)) return FS_ERR_READ_ONLY;
    *out = PSEUDO_FD_TAG | (uint32_t)id;
    return FS_OK;
}
int pseudo_fs_read(uint32_t id, uint32_t offset, char* b, uint32_t cap, uint32_t* n) {
    id &= ~PSEUDO_FD_TAG;
    return id < 6u ? devfs_read(id, offset, b, cap, n) :
           id < 10u ? procfs_read(id - 6u, offset, b, cap, n) : FS_ERR_INVALID;
}
int pseudo_fs_write(uint32_t id, uint32_t offset, const char* b, uint32_t len, uint32_t* n) {
    (void)offset;
    id &= ~PSEUDO_FD_TAG;
    return id < 6u ? devfs_write(id, b, len, n) : FS_ERR_READ_ONLY;
}
int pseudo_fs_size(uint32_t id, uint32_t* size) {
    id &= ~PSEUDO_FD_TAG;
    if (!size || id >= 10u) return FS_ERR_INVALID;
    if (id < 6u) { *size = 0u; return FS_OK; }
    return procfs_size(id - 6u, size);
}
int pseudo_fs_fstat(uint32_t id, fs_stat_t* st) { return stat_id((int)(id & ~PSEUDO_FD_TAG), st); }
int pseudo_fs_close(uint32_t id) { return (id & ~PSEUDO_FD_TAG) < 10u ? FS_OK : FS_ERR_INVALID; }
int pseudo_fs_read_file(const char* path, char* b, uint32_t cap, uint32_t* size) {
    uint32_t id;
    int rc = pseudo_fs_open(path, FS_O_RDONLY, &id);
    if (rc != FS_OK) return rc;
    return pseudo_fs_read(id, 0u, b, cap, size);
}
