#include "ramfs_core.h"

#define FS_MAX_NODES 64u
#define FS_MAX_CHILDREN 16u

typedef struct fs_node fs_node_t;

struct fs_node {
    uint8_t used;
    uint8_t type;
    char name[FS_NAME_CAP];
    fs_node_t* parent;
    fs_node_t* children[FS_MAX_CHILDREN];
    uint32_t child_count;
};

static fs_node_t g_nodes[FS_MAX_NODES];
static fs_node_t* g_root = 0;
static fs_node_t* g_cwd = 0;
static char g_path_buf[FS_PATH_CAP];
static uint8_t g_dirty = 0u;
static const char g_dot_name[] = ".";
static const char g_dotdot_name[] = "..";

static void fs_memzero(void* ptr, uint32_t size) {
    uint8_t* out = (uint8_t*)ptr;
    uint32_t i;

    for (i = 0u; i < size; ++i) {
        out[i] = 0u;
    }
}

static int fs_streq(const char* a, const char* b) {
    uint32_t i = 0u;

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        ++i;
    }

    return (int)(a[i] == '\0' && b[i] == '\0');
}

static char fs_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }

    return c;
}

static int fs_name_eq(const char* stored, const char* requested) {
    uint32_t i = 0u;
    uint32_t alias_len = 0u;

    while (stored[i] != '\0' && requested[i] != '\0') {
        if (fs_lower(stored[i]) != fs_lower(requested[i])) {
            break;
        }
        ++i;
    }

    if (stored[i] == '\0' && requested[i] == '\0') {
        return 1;
    }

    while (stored[alias_len] != '\0' && stored[alias_len] != '~') {
        ++alias_len;
    }

    if (stored[alias_len] != '~' || alias_len == 0u || requested[alias_len] == '\0') {
        return 0;
    }

    i = alias_len + 1u;
    while (stored[i] != '\0') {
        if (stored[i] < '0' || stored[i] > '9') {
            return 0;
        }
        ++i;
    }

    for (i = 0u; i < alias_len; ++i) {
        if (fs_lower(stored[i]) != fs_lower(requested[i])) {
            return 0;
        }
    }

    return 1;
}

static int fs_name_copy(char* dst, const char* src) {
    uint32_t i = 0u;

    if (src[0] == '\0') {
        return FS_ERR_INVALID;
    }

    while (src[i] != '\0') {
        if (src[i] == '/') {
            return FS_ERR_INVALID;
        }

        if (i + 1u >= FS_NAME_CAP) {
            return FS_ERR_INVALID;
        }

        dst[i] = src[i];
        ++i;
    }

    dst[i] = '\0';
    return FS_OK;
}

static fs_node_t* fs_alloc_node(void) {
    uint32_t i;

    for (i = 0u; i < FS_MAX_NODES; ++i) {
        if (g_nodes[i].used == 0u) {
            fs_memzero(&g_nodes[i], (uint32_t)sizeof(g_nodes[i]));
            g_nodes[i].used = 1u;
            return &g_nodes[i];
        }
    }

    return 0;
}

static fs_node_t* fs_find_child(fs_node_t* dir, const char* name) {
    uint32_t i;

    if (dir == 0 || dir->type != FS_NODE_DIR) {
        return 0;
    }

    for (i = 0u; i < dir->child_count; ++i) {
        if (fs_name_eq(dir->children[i]->name, name) != 0) {
            return dir->children[i];
        }
    }

    return 0;
}

static int fs_attach_child(fs_node_t* parent, fs_node_t* child) {
    if (parent == 0 || child == 0 || parent->type != FS_NODE_DIR) {
        return FS_ERR_INVALID;
    }

    if (parent->child_count >= FS_MAX_CHILDREN) {
        return FS_ERR_NO_SPACE;
    }

    parent->children[parent->child_count++] = child;
    child->parent = parent;
    return FS_OK;
}

static int fs_create_node(fs_node_t* parent, const char* name, uint8_t type, fs_node_t** out_node) {
    fs_node_t* node;
    int rc;

    if (parent == 0 || parent->type != FS_NODE_DIR) {
        return FS_ERR_NOT_DIR;
    }

    if (fs_find_child(parent, name) != 0) {
        return FS_ERR_EXISTS;
    }

    node = fs_alloc_node();
    if (node == 0) {
        return FS_ERR_NO_SPACE;
    }

    rc = fs_name_copy(node->name, name);
    if (rc != FS_OK) {
        node->used = 0u;
        return rc;
    }

    node->type = type;
    rc = fs_attach_child(parent, node);
    if (rc != FS_OK) {
        node->used = 0u;
        return rc;
    }

    if (out_node != 0) {
        *out_node = node;
    }

    return FS_OK;
}

static int fs_create_bootstrap_dir(fs_node_t* parent, const char* name, fs_node_t** out_dir) {
    return fs_create_node(parent, name, FS_NODE_DIR, out_dir);
}

static int fs_create_bootstrap_file(fs_node_t* parent, const char* name) {
    return fs_create_node(parent, name, FS_NODE_FILE, 0);
}

static int fs_next_segment(const char** io_path, char* segment) {
    uint32_t len = 0u;
    const char* path = *io_path;

    while (*path == '/') {
        ++path;
    }

    if (*path == '\0') {
        *io_path = path;
        segment[0] = '\0';
        return 0;
    }

    while (*path != '\0' && *path != '/') {
        if (len + 1u >= FS_NAME_CAP) {
            return FS_ERR_INVALID;
        }
        segment[len++] = *path;
        ++path;
    }

    segment[len] = '\0';
    *io_path = path;
    return 1;
}

static int fs_resolve_path_from(fs_node_t* start, const char* path, fs_node_t** out_node) {
    fs_node_t* current;
    char segment[FS_NAME_CAP];
    const char* cursor = path;
    int seg_status;

    if (path == 0 || out_node == 0) {
        return FS_ERR_INVALID;
    }

    current = (path[0] == '/') ? g_root : start;
    if (current == 0) {
        return FS_ERR_INVALID;
    }

    if (path[0] == '\0') {
        *out_node = current;
        return FS_OK;
    }

    for (;;) {
        seg_status = fs_next_segment(&cursor, segment);
        if (seg_status < 0) {
            return seg_status;
        }

        if (seg_status == 0) {
            *out_node = current;
            return FS_OK;
        }

        if (fs_streq(segment, ".") != 0) {
            continue;
        }

        if (fs_streq(segment, "..") != 0) {
            if (current->parent != 0) {
                current = current->parent;
            }
            continue;
        }

        if (current->type != FS_NODE_DIR) {
            return FS_ERR_NOT_DIR;
        }

        current = fs_find_child(current, segment);
        if (current == 0) {
            return FS_ERR_NOT_FOUND;
        }
    }
}

static int fs_resolve_path(const char* path, fs_node_t** out_node) {
    return fs_resolve_path_from(g_cwd, path, out_node);
}

static int fs_resolve_parent(const char* path, fs_node_t** out_parent, char* out_name) {
    fs_node_t* current;
    const char* cursor;
    char segment[FS_NAME_CAP];
    int seg_status;
    const char* lookahead;

    if (path == 0 || out_parent == 0 || out_name == 0) {
        return FS_ERR_INVALID;
    }

    current = (path[0] == '/') ? g_root : g_cwd;
    cursor = path;

    for (;;) {
        seg_status = fs_next_segment(&cursor, segment);
        if (seg_status <= 0) {
            return FS_ERR_INVALID;
        }

        lookahead = cursor;
        while (*lookahead == '/') {
            ++lookahead;
        }

        if (*lookahead == '\0') {
            if (fs_streq(segment, ".") != 0 || fs_streq(segment, "..") != 0) {
                return FS_ERR_INVALID;
            }

            *out_parent = current;
            return fs_name_copy(out_name, segment);
        }

        if (fs_streq(segment, ".") != 0) {
            continue;
        }

        if (fs_streq(segment, "..") != 0) {
            if (current->parent != 0) {
                current = current->parent;
            }
            continue;
        }

        current = fs_find_child(current, segment);
        if (current == 0) {
            return FS_ERR_NOT_FOUND;
        }

        if (current->type != FS_NODE_DIR) {
            return FS_ERR_NOT_DIR;
        }
    }
}

int ramfs_core_reset_empty(void) {
    fs_memzero(g_nodes, (uint32_t)sizeof(g_nodes));
    fs_memzero(g_path_buf, FS_PATH_CAP);
    g_dirty = 0u;

    g_root = fs_alloc_node();
    if (g_root == 0) {
        return FS_ERR_NO_SPACE;
    }

    g_root->type = FS_NODE_DIR;
    g_root->name[0] = '\0';
    g_root->parent = 0;
    g_cwd = g_root;
    return FS_OK;
}

int ramfs_core_init(void) {
    fs_node_t* boot = 0;
    fs_node_t* dev = 0;
    fs_node_t* etc = 0;
    fs_node_t* tmp = 0;

    if (ramfs_core_reset_empty() != FS_OK) {
        return FS_ERR_NO_SPACE;
    }

    if (fs_create_bootstrap_dir(g_root, "boot", &boot) != FS_OK ||
        fs_create_bootstrap_dir(g_root, "dev", &dev) != FS_OK ||
        fs_create_bootstrap_dir(g_root, "etc", &etc) != FS_OK ||
        fs_create_bootstrap_dir(g_root, "tmp", &tmp) != FS_OK) {
        return FS_ERR_NO_SPACE;
    }

    if (fs_create_bootstrap_file(boot, "kernel") != FS_OK ||
        fs_create_bootstrap_file(etc, "init") != FS_OK ||
        fs_create_bootstrap_file(dev, "tty0") != FS_OK) {
        return FS_ERR_NO_SPACE;
    }

    (void)tmp;
    return FS_OK;
}

static int ramfs_core_import_node(const char* path, uint8_t type) {
    fs_node_t* parent = 0;
    fs_node_t* existing = 0;
    char name[FS_NAME_CAP];
    int rc;

    if (path == 0 || path[0] != '/') {
        return FS_ERR_INVALID;
    }

    if (path[1] == '\0') {
        return (type == FS_NODE_DIR) ? FS_OK : FS_ERR_INVALID;
    }

    rc = fs_resolve_parent(path, &parent, name);
    if (rc != FS_OK) {
        return rc;
    }

    existing = fs_find_child(parent, name);
    if (existing != 0) {
        return (existing->type == type) ? FS_OK : FS_ERR_EXISTS;
    }

    return fs_create_node(parent, name, type, 0);
}

int ramfs_core_import_dir(const char* path) {
    return ramfs_core_import_node(path, FS_NODE_DIR);
}

int ramfs_core_import_file(const char* path) {
    return ramfs_core_import_node(path, FS_NODE_FILE);
}

uint8_t ramfs_core_is_dirty(void) {
    return g_dirty;
}

void ramfs_core_clear_dirty(void) {
    g_dirty = 0u;
}

const char* ramfs_core_get_cwd_path(void) {
    fs_node_t* chain[FS_PATH_CAP / 2u];
    uint32_t depth = 0u;
    uint32_t pos = 0u;
    uint32_t i;

    if (g_cwd == 0) {
        return "/";
    }

    if (g_cwd == g_root) {
        g_path_buf[0] = '/';
        g_path_buf[1] = '\0';
        return g_path_buf;
    }

    fs_memzero(g_path_buf, FS_PATH_CAP);

    while (g_cwd != 0 && depth < (FS_PATH_CAP / 2u)) {
        chain[depth] = (depth == 0u) ? g_cwd : chain[depth - 1u]->parent;
        ++depth;

        if (chain[depth - 1u] == g_root) {
            break;
        }

        if (depth >= (FS_PATH_CAP / 2u)) {
            break;
        }
    }

    for (i = depth; i > 0u; --i) {
        fs_node_t* node = chain[i - 1u];
        uint32_t j = 0u;

        if (node == g_root) {
            continue;
        }

        if (pos + 1u >= FS_PATH_CAP) {
            break;
        }

        g_path_buf[pos++] = '/';
        while (node->name[j] != '\0') {
            if (pos + 1u >= FS_PATH_CAP) {
                break;
            }
            g_path_buf[pos++] = node->name[j++];
        }
    }

    if (pos == 0u) {
        g_path_buf[pos++] = '/';
    }

    g_path_buf[pos] = '\0';
    return g_path_buf;
}

int ramfs_core_change_dir(const char* path) {
    fs_node_t* node = 0;
    int rc;

    rc = fs_resolve_path(path, &node);
    if (rc != FS_OK) {
        return rc;
    }

    if (node->type != FS_NODE_DIR) {
        return FS_ERR_NOT_DIR;
    }

    g_cwd = node;
    return FS_OK;
}

int ramfs_core_make_dir(const char* path) {
    fs_node_t* parent = 0;
    char name[FS_NAME_CAP];
    int rc;

    rc = fs_resolve_parent(path, &parent, name);
    if (rc != FS_OK) {
        return rc;
    }

    rc = fs_create_node(parent, name, FS_NODE_DIR, 0);
    if (rc == FS_OK) {
        g_dirty = 1u;
    }

    return rc;
}

int ramfs_core_list_dir(const char* path, fs_dirent_t* entries, uint32_t cap, uint32_t* out_count) {
    fs_node_t* dir = 0;
    uint32_t i;
    uint32_t pos = 0u;
    uint32_t special_count = 0u;
    int rc;

    if (entries == 0 || out_count == 0) {
        return FS_ERR_INVALID;
    }

    rc = fs_resolve_path((path == 0) ? "" : path, &dir);
    if (rc != FS_OK) {
        return rc;
    }

    if (dir->type != FS_NODE_DIR) {
        return FS_ERR_NOT_DIR;
    }

    if (dir != g_root) {
        special_count = 2u;
    }

    *out_count = dir->child_count + special_count;
    if (cap < *out_count) {
        return FS_ERR_NO_SPACE;
    }

    if (dir != g_root) {
        entries[pos].name = g_dot_name;
        entries[pos].type = FS_NODE_DIR;
        ++pos;
        entries[pos].name = g_dotdot_name;
        entries[pos].type = FS_NODE_DIR;
        ++pos;
    }

    for (i = 0u; i < dir->child_count; ++i) {
        entries[pos].name = dir->children[i]->name;
        entries[pos].type = dir->children[i]->type;
        ++pos;
    }

    return FS_OK;
}

int ramfs_core_read_file(const char* path, char* buffer, uint32_t cap, uint32_t* out_size) {
    fs_node_t* node = 0;
    int rc;

    if (path == 0 || buffer == 0 || out_size == 0) {
        return FS_ERR_INVALID;
    }

    rc = fs_resolve_path(path, &node);
    if (rc != FS_OK) {
        return rc;
    }

    if (node->type != FS_NODE_FILE) {
        return FS_ERR_NOT_DIR;
    }

    (void)cap;
    *out_size = 0u;
    return FS_ERR_INVALID;
}

static int ramfs_core_driver_init(void) {
    return (g_root == 0) ? FS_ERR_INVALID : FS_OK;
}

static const vfs_driver_t g_ramfs_driver = {
    ramfs_core_driver_init,
    ramfs_core_get_cwd_path,
    ramfs_core_change_dir,
    ramfs_core_make_dir,
    ramfs_core_list_dir,
    ramfs_core_read_file
};

const vfs_driver_t* ramfs_core_driver(void) {
    return &g_ramfs_driver;
}
