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
        if (fs_streq(dir->children[i]->name, name) != 0) {
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

int ramfs_core_init(void) {
    fs_node_t* boot = 0;
    fs_node_t* dev = 0;
    fs_node_t* etc = 0;
    fs_node_t* tmp = 0;

    fs_memzero(g_nodes, (uint32_t)sizeof(g_nodes));
    fs_memzero(g_path_buf, FS_PATH_CAP);

    g_root = fs_alloc_node();
    if (g_root == 0) {
        return FS_ERR_NO_SPACE;
    }

    g_root->type = FS_NODE_DIR;
    g_root->name[0] = '\0';
    g_root->parent = 0;
    g_cwd = g_root;

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

    return fs_create_node(parent, name, FS_NODE_DIR, 0);
}

int ramfs_core_list_dir(const char* path, fs_dirent_t* entries, uint32_t cap, uint32_t* out_count) {
    fs_node_t* dir = 0;
    uint32_t i;
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

    *out_count = dir->child_count;
    if (cap < dir->child_count) {
        return FS_ERR_NO_SPACE;
    }

    for (i = 0u; i < dir->child_count; ++i) {
        entries[i].name = dir->children[i]->name;
        entries[i].type = dir->children[i]->type;
    }

    return FS_OK;
}
