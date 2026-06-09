#include "iso9660_core.h"

#define ISO9660_PVD_LBA 16u
#define ISO9660_SECTOR_SIZE 2048u
#define ISO9660_NAME_SLOTS 32u

static block_device_t* g_iso_dev = 0;
static uint32_t g_iso_root_lba = 0u;
static uint32_t g_iso_root_size = 0u;
static char g_iso_names[ISO9660_NAME_SLOTS][FS_NAME_CAP];
static char g_iso_cwd[FS_PATH_CAP];
static uint32_t g_iso_cwd_lba = 0u;
static uint32_t g_iso_cwd_size = 0u;
static uint8_t g_iso_sector[ISO9660_SECTOR_SIZE];

static uint32_t iso_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int iso_streq(const char* a, const char* b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }

    return (int)(*a == '\0' && *b == '\0');
}

static char iso_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }

    return c;
}

static int iso_name_eq(const char* a, const char* b) {
    uint32_t i = 0u;
    uint32_t alias_len = 0u;

    while (a[i] != '\0' && b[i] != '\0') {
        if (iso_lower(a[i]) != iso_lower(b[i])) {
            break;
        }
        ++i;
    }

    if (a[i] == '\0' && b[i] == '\0') {
        return 1;
    }

    while (a[alias_len] != '\0' && a[alias_len] != '~') {
        ++alias_len;
    }

    if (a[alias_len] != '~' || alias_len == 0u || b[alias_len] == '\0') {
        return 0;
    }

    i = alias_len + 1u;
    while (a[i] != '\0') {
        if (a[i] < '0' || a[i] > '9') {
            return 0;
        }
        ++i;
    }

    for (i = 0u; i < alias_len; ++i) {
        if (iso_lower(a[i]) != iso_lower(b[i])) {
            return 0;
        }
    }

    return 1;
}

static void iso_copy_string(char* dst, uint32_t cap, const char* src) {
    uint32_t i = 0u;

    if (cap == 0u) {
        return;
    }

    while (src[i] != '\0' && (i + 1u) < cap) {
        dst[i] = src[i];
        ++i;
    }

    dst[i] = '\0';
}

static void iso_clear_names(void) {
    uint32_t i;

    for (i = 0u; i < ISO9660_NAME_SLOTS; ++i) {
        g_iso_names[i][0] = '\0';
    }
}

static int iso_read_2048(block_device_t* dev, uint32_t iso_lba, uint8_t* buffer) {
    uint32_t base = iso_lba * 4u;

    if (dev == 0 || dev->sector_size != 512u) {
        return FS_ERR_INVALID;
    }

    return block_core_read(dev, base, 4u, buffer);
}

static void iso_copy_name(char* out, const uint8_t* name, uint8_t len) {
    uint32_t pos = 0u;
    uint32_t i;

    for (i = 0u; i < (uint32_t)len && (pos + 1u) < FS_NAME_CAP; ++i) {
        if (name[i] == ';') {
            break;
        }
        out[pos++] = iso_lower((char)name[i]);
    }

    while (pos > 0u && out[pos - 1u] == '.') {
        --pos;
    }

    out[pos] = '\0';
}

static int iso_copy_rock_ridge_name(char* out, const uint8_t* record, uint8_t record_len, uint8_t name_len) {
    uint32_t offset = 33u + (uint32_t)name_len;
    uint32_t pos = 0u;

    if ((name_len & 1u) == 0u) {
        ++offset;
    }

    while (offset + 4u <= (uint32_t)record_len) {
        const uint8_t* field = record + offset;
        uint8_t field_len = field[2u];
        uint32_t i;

        if (field_len < 4u || offset + (uint32_t)field_len > (uint32_t)record_len) {
            break;
        }

        if (field[0] == 'N' && field[1] == 'M' && field_len >= 5u) {
            uint8_t flags = field[4u];

            if ((flags & 0x06u) != 0u) {
                return 0;
            }

            for (i = 5u; i < (uint32_t)field_len && (pos + 1u) < FS_NAME_CAP; ++i) {
                out[pos++] = (char)field[i];
            }
            out[pos] = '\0';
            return (pos > 0u) ? 1 : 0;
        }

        offset += (uint32_t)field_len;
    }

    return 0;
}

static void iso_copy_record_name(char* out, const uint8_t* record, uint8_t record_len) {
    uint8_t name_len = record[32u];

    if (iso_copy_rock_ridge_name(out, record, record_len, name_len) != 0) {
        return;
    }

    iso_copy_name(out, record + 33u, name_len);
}

int iso9660_core_mount(block_device_t* dev) {
    int rc;
    uint8_t* root_record;

    rc = iso_read_2048(dev, ISO9660_PVD_LBA, g_iso_sector);
    if (rc != FS_OK) {
        return rc;
    }

    if (g_iso_sector[0] != 1u ||
        g_iso_sector[1] != 'C' ||
        g_iso_sector[2] != 'D' ||
        g_iso_sector[3] != '0' ||
        g_iso_sector[4] != '0' ||
        g_iso_sector[5] != '1') {
        return FS_ERR_INVALID;
    }

    root_record = g_iso_sector + 156u;
    if (root_record[0] == 0u) {
        return FS_ERR_INVALID;
    }

    g_iso_root_lba = iso_le32(root_record + 2u);
    g_iso_root_size = iso_le32(root_record + 10u);
    if (g_iso_root_lba == 0u || g_iso_root_size == 0u) {
        return FS_ERR_INVALID;
    }

    g_iso_dev = dev;
    g_iso_cwd[0] = '/';
    g_iso_cwd[1] = '\0';
    g_iso_cwd_lba = g_iso_root_lba;
    g_iso_cwd_size = g_iso_root_size;
    return FS_OK;
}

static int iso_init(void) {
    return (g_iso_dev == 0) ? FS_ERR_INVALID : FS_OK;
}

static const char* iso_get_cwd_path(void) {
    return g_iso_cwd;
}

static int iso_find_entry_in_dir(uint32_t dir_lba, uint32_t dir_size, const char* name, uint32_t* out_lba, uint32_t* out_size, uint8_t* out_flags) {
    char entry_name[FS_NAME_CAP];
    int rc;

    uint32_t total_sectors = (dir_size + 2047u) / 2048u;
    uint32_t sector_idx;

    for (sector_idx = 0u; sector_idx < total_sectors; ++sector_idx) {
        rc = iso_read_2048(g_iso_dev, dir_lba + sector_idx, g_iso_sector);
        if (rc != FS_OK) {
            return rc;
        }

        uint32_t offset = 0u;
        while (offset < 2048u) {
            uint8_t len = g_iso_sector[offset];
            if (len == 0u) {
                break;
            }

            const uint8_t* record = g_iso_sector + offset;
            uint8_t flags = record[25u];
            uint8_t name_len = record[32u];

            if (name_len == 1u && record[33u] == 0u) {
                if (iso_streq(name, ".") != 0) {
                    if (out_lba) *out_lba = iso_le32(record + 2u);
                    if (out_size) *out_size = iso_le32(record + 10u);
                    if (out_flags) *out_flags = flags;
                    return FS_OK;
                }
            } else if (name_len == 1u && record[33u] == 1u) {
                if (iso_streq(name, "..") != 0) {
                    if (out_lba) *out_lba = iso_le32(record + 2u);
                    if (out_size) *out_size = iso_le32(record + 10u);
                    if (out_flags) *out_flags = flags;
                    return FS_OK;
                }
            } else {
                iso_copy_record_name(entry_name, record, len);
                if (iso_name_eq(entry_name, name) != 0) {
                    if (out_lba) *out_lba = iso_le32(record + 2u);
                    if (out_size) *out_size = iso_le32(record + 10u);
                    if (out_flags) *out_flags = flags;
                    return FS_OK;
                }
            }

            offset += len;
        }
    }

    return FS_ERR_NOT_FOUND;
}

static int iso_resolve_path(const char* path, uint32_t* out_lba, uint32_t* out_size, uint8_t* out_is_dir) {
    uint8_t current_is_dir = 1u;

    if (path == 0 || path[0] == '\0') {
        return FS_ERR_INVALID;
    }

    uint32_t current_lba = g_iso_cwd_lba;
    uint32_t current_size = g_iso_cwd_size;
    const char* cursor = path;

    if (path[0] == '/') {
        current_lba = g_iso_root_lba;
        current_size = g_iso_root_size;
        while (*cursor == '/') {
            ++cursor;
        }
    }

    char component[FS_NAME_CAP];
    while (*cursor != '\0') {
        uint32_t len = 0u;
        while (*cursor != '\0' && *cursor != '/') {
            if (len + 1u >= FS_NAME_CAP) {
                return FS_ERR_INVALID;
            }
            component[len++] = *cursor++;
        }
        component[len] = '\0';

        while (*cursor == '/') {
            ++cursor;
        }

        if (len == 0u || iso_streq(component, ".") != 0) {
            continue;
        }

        if (iso_streq(component, "..") != 0 && current_lba == g_iso_root_lba) {
            continue;
        }

        uint32_t next_lba = 0u;
        uint32_t next_size = 0u;
        uint8_t flags = 0u;
        int rc = iso_find_entry_in_dir(current_lba, current_size, component, &next_lba, &next_size, &flags);
        if (rc != FS_OK) {
            return rc;
        }

        if (*cursor != '\0') {
            if ((flags & 0x02u) == 0u) {
                return FS_ERR_NOT_DIR;
            }
        }

        current_lba = next_lba;
        current_size = next_size;
        current_is_dir = ((flags & 0x02u) != 0u) ? 1u : 0u;
        if (out_is_dir) {
            *out_is_dir = current_is_dir;
        }
    }

    if (out_lba) {
        *out_lba = current_lba;
    }
    if (out_size) {
        *out_size = current_size;
    }
    if (out_is_dir) {
        *out_is_dir = current_is_dir;
    }

    return FS_OK;
}

static void iso_normalize_path(char* dst, const char* path) {
    char temp[FS_PATH_CAP];

    if (path[0] == '/') {
        temp[0] = '\0';
    } else {
        iso_copy_string(temp, FS_PATH_CAP, g_iso_cwd);
    }

    const char* cursor = path;
    if (path[0] == '/') {
        while (*cursor == '/') ++cursor;
    }

    while (*cursor != '\0') {
        char comp[FS_NAME_CAP];
        uint32_t comp_len = 0u;
        while (*cursor != '\0' && *cursor != '/') {
            if (comp_len + 1u < FS_NAME_CAP) {
                comp[comp_len++] = *cursor;
            }
            ++cursor;
        }
        comp[comp_len] = '\0';
        while (*cursor == '/') ++cursor;

        if (comp_len == 0u || iso_streq(comp, ".") != 0) {
            continue;
        }

        if (iso_streq(comp, "..") != 0) {
            uint32_t len = 0u;
            while (temp[len] != '\0') ++len;
            while (len > 0u && temp[len - 1u] != '/') {
                --len;
            }
            if (len > 1u && temp[len - 1u] == '/') {
                temp[len - 1u] = '\0';
            } else if (len == 1u) {
                temp[1] = '\0';
            } else {
                temp[0] = '\0';
            }
        } else {
            uint32_t len = 0u;
            while (temp[len] != '\0') ++len;
            if (len == 0u || temp[len - 1u] != '/') {
                if (len + 1u < FS_PATH_CAP) {
                    temp[len++] = '/';
                    temp[len] = '\0';
                }
            }
            uint32_t i = 0u;
            while (comp[i] != '\0' && len + 1u < FS_PATH_CAP) {
                temp[len++] = comp[i++];
            }
            temp[len] = '\0';
        }
    }

    if (temp[0] == '\0') {
        dst[0] = '/';
        dst[1] = '\0';
    } else {
        iso_copy_string(dst, FS_PATH_CAP, temp);
    }
}

static int iso_change_dir(const char* path) {
    if (path == 0 || path[0] == '\0') {
        return FS_ERR_INVALID;
    }

    uint32_t target_lba = 0u;
    uint32_t target_size = 0u;
    uint8_t is_dir = 0u;
    int rc = iso_resolve_path(path, &target_lba, &target_size, &is_dir);
    if (rc != FS_OK) {
        return rc;
    }

    if (is_dir == 0u) {
        return FS_ERR_NOT_DIR;
    }

    g_iso_cwd_lba = target_lba;
    g_iso_cwd_size = target_size;

    char new_path[FS_PATH_CAP];
    iso_normalize_path(new_path, path);
    iso_copy_string(g_iso_cwd, FS_PATH_CAP, new_path);

    return FS_OK;
}

static int iso_make_dir(const char* path) {
    (void)path;
    return FS_ERR_READ_ONLY;
}

static int iso_read_file(const char* path, char* buffer, uint32_t cap, uint32_t* out_size) {
    uint32_t target_lba = 0u;
    uint32_t target_size = 0u;
    uint8_t is_dir = 0u;
    uint32_t remaining;
    uint32_t copied = 0u;
    uint32_t sector_idx = 0u;
    int rc;

    if (buffer == 0 || out_size == 0) {
        return FS_ERR_INVALID;
    }

    rc = iso_resolve_path(path, &target_lba, &target_size, &is_dir);
    if (rc != FS_OK) {
        return rc;
    }

    if (is_dir != 0u) {
        return FS_ERR_NOT_DIR;
    }

    *out_size = target_size;
    if (cap < target_size) {
        return FS_ERR_NO_SPACE;
    }

    remaining = target_size;
    while (remaining > 0u) {
        uint32_t chunk = (remaining > ISO9660_SECTOR_SIZE) ? ISO9660_SECTOR_SIZE : remaining;
        uint32_t i;

        rc = iso_read_2048(g_iso_dev, target_lba + sector_idx, g_iso_sector);
        if (rc != FS_OK) {
            return rc;
        }

        for (i = 0u; i < chunk; ++i) {
            buffer[copied + i] = (char)g_iso_sector[i];
        }

        copied += chunk;
        remaining -= chunk;
        ++sector_idx;
    }

    return FS_OK;
}

static int iso_list_dir(const char* path, fs_dirent_t* entries, uint32_t cap, uint32_t* out_count) {
    uint32_t count = 0u;
    int rc;

    if (entries == 0 || out_count == 0) {
        return FS_ERR_INVALID;
    }

    uint32_t target_lba = g_iso_cwd_lba;
    uint32_t target_size = g_iso_cwd_size;
    if (path != 0 && path[0] != '\0' && iso_streq(path, ".") == 0) {
        uint8_t is_dir = 0u;
        rc = iso_resolve_path(path, &target_lba, &target_size, &is_dir);
        if (rc != FS_OK) {
            return rc;
        }
        if (!is_dir) {
            return FS_ERR_NOT_DIR;
        }
    }

    iso_clear_names();

    uint32_t total_sectors = (target_size + 2047u) / 2048u;
    uint32_t sector_idx;

    for (sector_idx = 0u; sector_idx < total_sectors; ++sector_idx) {
        rc = iso_read_2048(g_iso_dev, target_lba + sector_idx, g_iso_sector);
        if (rc != FS_OK) {
            return rc;
        }

        uint32_t offset = 0u;
        while (offset < 2048u) {
            uint8_t len = g_iso_sector[offset];
            if (len == 0u) {
                break;
            }

            const uint8_t* record = g_iso_sector + offset;
            uint8_t flags = record[25u];
            uint8_t name_len = record[32u];

            if (name_len == 1u && (record[33u] == 0u || record[33u] == 1u)) {
                offset += len;
                continue;
            }

            if (count >= cap || count >= ISO9660_NAME_SLOTS) {
                *out_count = count;
                return FS_ERR_NO_SPACE;
            }

            iso_copy_record_name(g_iso_names[count], record, len);
            entries[count].name = g_iso_names[count];
            entries[count].type = ((flags & 0x02u) != 0u) ? FS_NODE_DIR : FS_NODE_FILE;
            ++count;
            offset += len;
        }
    }

    *out_count = count;
    return FS_OK;
}

static const vfs_driver_t g_iso_driver = {
    iso_init,
    iso_get_cwd_path,
    iso_change_dir,
    iso_make_dir,
    iso_list_dir,
    iso_read_file
};

const vfs_driver_t* iso9660_core_driver(void) {
    return &g_iso_driver;
}
