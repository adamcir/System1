#include "fat12_core.h"

#define FAT12_ATTR_VOLUME_ID 0x08u
#define FAT12_ATTR_DIRECTORY 0x10u
#define FAT12_ENTRY_FREE 0x00u
#define FAT12_ENTRY_DELETED 0xE5u
#define FAT12_NAME_SLOTS 32u
#define FAT12_CLUSTER_FREE 0x000u
#define FAT12_CLUSTER_EOC 0xFFFu

typedef struct {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entry_count;
    uint16_t sectors_per_fat;
    uint32_t root_dir_lba;
    uint32_t root_dir_sectors;
} fat12_bpb_t;

static block_device_t* g_fat12_dev = 0;
static fat12_bpb_t g_fat12;
static char g_fat12_names[FAT12_NAME_SLOTS][FS_NAME_CAP];
static char g_fat12_cwd[FS_PATH_CAP];
static uint32_t g_fat12_cwd_cluster = 0u;
static uint8_t g_fat12_sector[512];

static uint16_t fat12_le16(const uint8_t* p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t fat12_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void fat12_put_le16(uint8_t* p, uint16_t value) {
    p[0] = (uint8_t)(value & 0xFFu);
    p[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void fat12_clear_names(void) {
    uint32_t i;

    for (i = 0u; i < FAT12_NAME_SLOTS; ++i) {
        g_fat12_names[i][0] = '\0';
    }
}

static int fat12_streq(const char* a, const char* b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }

    return (int)(*a == '\0' && *b == '\0');
}

static char fat12_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }

    return c;
}

static int fat12_name_eq(const char* a, const char* b) {
    while (*a != '\0' && *b != '\0') {
        if (fat12_lower(*a) != fat12_lower(*b)) {
            return 0;
        }
        ++a;
        ++b;
    }

    return (int)(*a == '\0' && *b == '\0');
}

static void fat12_copy_string(char* dst, uint32_t cap, const char* src) {
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

static int fat12_extract_root_name(const char* path, char* out) {
    uint32_t pos = 0u;
    const char* cursor = path;

    if (path == 0 || path[0] == '\0' || fat12_streq(path, ".") != 0) {
        out[0] = '\0';
        return FS_OK;
    }

    while (*cursor == '/') {
        ++cursor;
    }

    while (*cursor != '\0' && *cursor != '/') {
        if ((pos + 1u) >= FS_NAME_CAP) {
            return FS_ERR_INVALID;
        }
        out[pos++] = fat12_lower(*cursor);
        ++cursor;
    }

    while (*cursor == '/') {
        ++cursor;
    }

    if (*cursor != '\0') {
        return FS_ERR_NOT_FOUND;
    }

    out[pos] = '\0';
    return FS_OK;
}

static void fat12_copy_83_name(char* out, const uint8_t* entry) {
    uint32_t pos = 0u;
    uint32_t i;
    uint32_t end = 8u;

    while (end > 0u && entry[end - 1u] == ' ') {
        --end;
    }

    for (i = 0u; i < end && (pos + 1u) < FS_NAME_CAP; ++i) {
        out[pos++] = fat12_lower((char)entry[i]);
    }

    end = 11u;
    while (end > 8u && entry[end - 1u] == ' ') {
        --end;
    }

    if (end > 8u && (pos + 1u) < FS_NAME_CAP) {
        out[pos++] = '.';
        for (i = 8u; i < end && (pos + 1u) < FS_NAME_CAP; ++i) {
            out[pos++] = fat12_lower((char)entry[i]);
        }
    }

    out[pos] = '\0';
}

static int fat12_root_dir_exists(const char* name) {
    uint32_t sector_index;
    uint32_t entry_index;
    char entry_name[FS_NAME_CAP];
    int rc;

    for (sector_index = 0u; sector_index < g_fat12.root_dir_sectors; ++sector_index) {
        rc = block_core_read(g_fat12_dev, g_fat12.root_dir_lba + sector_index, 1u, g_fat12_sector);
        if (rc != FS_OK) {
            return rc;
        }

        for (entry_index = 0u; entry_index < 16u; ++entry_index) {
            const uint8_t* entry = g_fat12_sector + (entry_index * 32u);
            uint8_t first = entry[0];
            uint8_t attr = entry[11];

            if (first == FAT12_ENTRY_FREE) {
                return FS_ERR_NOT_FOUND;
            }

            if (first == FAT12_ENTRY_DELETED ||
                (attr & FAT12_ATTR_VOLUME_ID) != 0u ||
                (attr & FAT12_ATTR_DIRECTORY) == 0u) {
                continue;
            }

            fat12_copy_83_name(entry_name, entry);
            if (fat12_name_eq(entry_name, name) != 0) {
                return FS_OK;
            }
        }
    }

    return FS_ERR_NOT_FOUND;
}

static int fat12_parse_bpb(const uint8_t* sector, fat12_bpb_t* out) {
    uint16_t total16;
    uint32_t total32;

    out->bytes_per_sector = fat12_le16(sector + 11u);
    out->sectors_per_cluster = sector[13u];
    out->reserved_sectors = fat12_le16(sector + 14u);
    out->fat_count = sector[16u];
    out->root_entry_count = fat12_le16(sector + 17u);
    total16 = fat12_le16(sector + 19u);
    total32 = fat12_le32(sector + 32u);
    out->sectors_per_fat = fat12_le16(sector + 22u);

    if (out->bytes_per_sector != 512u ||
        out->sectors_per_cluster == 0u ||
        out->reserved_sectors == 0u ||
        out->fat_count == 0u ||
        out->root_entry_count == 0u ||
        out->sectors_per_fat == 0u ||
        (total16 == 0u && total32 == 0u)) {
        return FS_ERR_INVALID;
    }

    out->root_dir_lba = (uint32_t)out->reserved_sectors + ((uint32_t)out->fat_count * (uint32_t)out->sectors_per_fat);
    out->root_dir_sectors = (((uint32_t)out->root_entry_count * 32u) + 511u) / 512u;
    return FS_OK;
}

static uint8_t* fat12_image_sector(uint8_t* image, uint32_t image_size, uint32_t lba) {
    uint32_t offset = lba * 512u;

    if (image == 0 || offset >= image_size || 512u > (image_size - offset)) {
        return 0;
    }

    return image + offset;
}

static uint32_t fat12_image_cluster_to_lba(const fat12_bpb_t* bpb, uint32_t cluster) {
    if (cluster < 2u) {
        return bpb->root_dir_lba;
    }

    return bpb->root_dir_lba + bpb->root_dir_sectors + (cluster - 2u) * (uint32_t)bpb->sectors_per_cluster;
}

static uint32_t fat12_image_total_sectors(const uint8_t* image) {
    uint16_t total16 = fat12_le16(image + 19u);
    uint32_t total32 = fat12_le32(image + 32u);

    return (total16 != 0u) ? (uint32_t)total16 : total32;
}

static uint32_t fat12_image_total_clusters(const fat12_bpb_t* bpb, const uint8_t* image) {
    uint32_t total_sectors = fat12_image_total_sectors(image);
    uint32_t data_lba = bpb->root_dir_lba + bpb->root_dir_sectors;

    if (total_sectors <= data_lba || bpb->sectors_per_cluster == 0u) {
        return 0u;
    }

    return (total_sectors - data_lba) / (uint32_t)bpb->sectors_per_cluster;
}

static uint16_t fat12_image_get_fat_entry(uint8_t* image, const fat12_bpb_t* bpb, uint32_t cluster) {
    uint32_t fat_offset = cluster + (cluster / 2u);
    uint8_t* fat = image + ((uint32_t)bpb->reserved_sectors * 512u);
    uint16_t value = (uint16_t)(fat[fat_offset] | ((uint16_t)fat[fat_offset + 1u] << 8));

    if ((cluster & 1u) != 0u) {
        value >>= 4;
    } else {
        value &= 0x0FFFu;
    }

    return value;
}

static void fat12_image_set_fat_entry(uint8_t* image, const fat12_bpb_t* bpb, uint32_t cluster, uint16_t value) {
    uint32_t fat_index;
    uint32_t fat_offset = cluster + (cluster / 2u);

    for (fat_index = 0u; fat_index < (uint32_t)bpb->fat_count; ++fat_index) {
        uint8_t* fat = image + (((uint32_t)bpb->reserved_sectors +
            fat_index * (uint32_t)bpb->sectors_per_fat) * 512u);

        if ((cluster & 1u) != 0u) {
            fat[fat_offset] = (uint8_t)((fat[fat_offset] & 0x0Fu) | ((value << 4) & 0xF0u));
            fat[fat_offset + 1u] = (uint8_t)((value >> 4) & 0xFFu);
        } else {
            fat[fat_offset] = (uint8_t)(value & 0xFFu);
            fat[fat_offset + 1u] = (uint8_t)((fat[fat_offset + 1u] & 0xF0u) | ((value >> 8) & 0x0Fu));
        }
    }
}

static int fat12_make_83_dir_name(const char* name, uint8_t out[11]) {
    uint32_t i;
    uint32_t len = 0u;

    if (name == 0 || name[0] == '\0') {
        return FS_ERR_INVALID;
    }

    for (i = 0u; i < 11u; ++i) {
        out[i] = ' ';
    }

    while (name[len] != '\0') {
        char c = name[len];

        if (c == '/' || c == '.' || len >= 8u) {
            return FS_ERR_INVALID;
        }

        if (c >= 'a' && c <= 'z') {
            c = (char)(c - ('a' - 'A'));
        }

        out[len] = (uint8_t)c;
        ++len;
    }

    return FS_OK;
}

static int fat12_split_create_path(const char* path, char* parent, char* name) {
    uint32_t len = 0u;
    uint32_t slash = 0u;
    uint32_t i;
    uint32_t out = 0u;

    if (path == 0 || path[0] != '/') {
        return FS_ERR_INVALID;
    }

    while (path[len] != '\0') {
        if (len + 1u >= FS_PATH_CAP) {
            return FS_ERR_INVALID;
        }
        if (path[len] == '/') {
            slash = len;
        }
        ++len;
    }

    if (len <= 1u || slash == len - 1u) {
        return FS_ERR_INVALID;
    }

    if (slash == 0u) {
        parent[0] = '/';
        parent[1] = '\0';
    } else {
        for (i = 0u; i < slash && i + 1u < FS_PATH_CAP; ++i) {
            parent[i] = path[i];
        }
        parent[i] = '\0';
    }

    for (i = slash + 1u; i < len; ++i) {
        if (out + 1u >= FS_NAME_CAP) {
            return FS_ERR_INVALID;
        }
        name[out++] = path[i];
    }
    name[out] = '\0';

    return FS_OK;
}

static int fat12_image_find_in_dir(uint8_t* image, uint32_t image_size, const fat12_bpb_t* bpb,
                                   uint32_t dir_cluster, const uint8_t name83[11],
                                   uint32_t* out_cluster, uint8_t* out_attr) {
    uint32_t current_cluster = dir_cluster;

    if (dir_cluster == 0u) {
        uint32_t sector_index;
        uint32_t entry_index;

        for (sector_index = 0u; sector_index < bpb->root_dir_sectors; ++sector_index) {
            uint8_t* sector = fat12_image_sector(image, image_size, bpb->root_dir_lba + sector_index);
            if (sector == 0) {
                return FS_ERR_INVALID;
            }

            for (entry_index = 0u; entry_index < 16u; ++entry_index) {
                uint8_t* entry = sector + entry_index * 32u;
                uint32_t i;
                uint8_t first = entry[0];
                uint8_t attr = entry[11];
                uint8_t same = 1u;

                if (first == FAT12_ENTRY_FREE) {
                    return FS_ERR_NOT_FOUND;
                }

                if (first == FAT12_ENTRY_DELETED || (attr & FAT12_ATTR_VOLUME_ID) != 0u) {
                    continue;
                }

                for (i = 0u; i < 11u; ++i) {
                    if (entry[i] != name83[i]) {
                        same = 0u;
                        break;
                    }
                }

                if (same != 0u) {
                    if (out_cluster != 0) {
                        *out_cluster = fat12_le16(entry + 26u);
                    }
                    if (out_attr != 0) {
                        *out_attr = attr;
                    }
                    return FS_OK;
                }
            }
        }

        return FS_ERR_NOT_FOUND;
    }

    while (current_cluster < 0xFF8u) {
        uint32_t sector_idx;
        uint32_t base_lba = fat12_image_cluster_to_lba(bpb, current_cluster);

        for (sector_idx = 0u; sector_idx < bpb->sectors_per_cluster; ++sector_idx) {
            uint8_t* sector = fat12_image_sector(image, image_size, base_lba + sector_idx);
            uint32_t entry_index;
            if (sector == 0) {
                return FS_ERR_INVALID;
            }

            for (entry_index = 0u; entry_index < 16u; ++entry_index) {
                uint8_t* entry = sector + entry_index * 32u;
                uint32_t i;
                uint8_t first = entry[0];
                uint8_t attr = entry[11];
                uint8_t same = 1u;

                if (first == FAT12_ENTRY_FREE) {
                    return FS_ERR_NOT_FOUND;
                }

                if (first == FAT12_ENTRY_DELETED || (attr & FAT12_ATTR_VOLUME_ID) != 0u) {
                    continue;
                }

                for (i = 0u; i < 11u; ++i) {
                    if (entry[i] != name83[i]) {
                        same = 0u;
                        break;
                    }
                }

                if (same != 0u) {
                    if (out_cluster != 0) {
                        *out_cluster = fat12_le16(entry + 26u);
                    }
                    if (out_attr != 0) {
                        *out_attr = attr;
                    }
                    return FS_OK;
                }
            }
        }

        current_cluster = fat12_image_get_fat_entry(image, bpb, current_cluster);
    }

    return FS_ERR_NOT_FOUND;
}

static int fat12_image_resolve_dir(uint8_t* image, uint32_t image_size, const fat12_bpb_t* bpb,
                                   const char* path, uint32_t* out_cluster) {
    uint32_t current_cluster = 0u;
    const char* cursor = path;
    char component[FS_NAME_CAP];

    if (path == 0 || path[0] != '/') {
        return FS_ERR_INVALID;
    }

    while (*cursor == '/') {
        ++cursor;
    }

    while (*cursor != '\0') {
        uint32_t len = 0u;
        uint8_t name83[11];
        uint32_t next_cluster = 0u;
        uint8_t attr = 0u;
        int rc;

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

        if (len == 0u) {
            continue;
        }

        rc = fat12_make_83_dir_name(component, name83);
        if (rc != FS_OK) {
            return rc;
        }

        rc = fat12_image_find_in_dir(image, image_size, bpb, current_cluster, name83, &next_cluster, &attr);
        if (rc != FS_OK) {
            return rc;
        }

        if ((attr & FAT12_ATTR_DIRECTORY) == 0u) {
            return FS_ERR_NOT_DIR;
        }

        current_cluster = next_cluster;
    }

    if (out_cluster != 0) {
        *out_cluster = current_cluster;
    }
    return FS_OK;
}

static uint8_t* fat12_image_find_free_entry(uint8_t* image, uint32_t image_size, const fat12_bpb_t* bpb,
                                            uint32_t dir_cluster) {
    uint32_t current_cluster = dir_cluster;

    if (dir_cluster == 0u) {
        uint32_t sector_index;
        uint32_t entry_index;

        for (sector_index = 0u; sector_index < bpb->root_dir_sectors; ++sector_index) {
            uint8_t* sector = fat12_image_sector(image, image_size, bpb->root_dir_lba + sector_index);
            if (sector == 0) {
                return 0;
            }

            for (entry_index = 0u; entry_index < 16u; ++entry_index) {
                uint8_t* entry = sector + entry_index * 32u;
                if (entry[0] == FAT12_ENTRY_FREE || entry[0] == FAT12_ENTRY_DELETED) {
                    return entry;
                }
            }
        }

        return 0;
    }

    while (current_cluster < 0xFF8u) {
        uint32_t sector_idx;
        uint32_t base_lba = fat12_image_cluster_to_lba(bpb, current_cluster);

        for (sector_idx = 0u; sector_idx < bpb->sectors_per_cluster; ++sector_idx) {
            uint8_t* sector = fat12_image_sector(image, image_size, base_lba + sector_idx);
            uint32_t entry_index;
            if (sector == 0) {
                return 0;
            }

            for (entry_index = 0u; entry_index < 16u; ++entry_index) {
                uint8_t* entry = sector + entry_index * 32u;
                if (entry[0] == FAT12_ENTRY_FREE || entry[0] == FAT12_ENTRY_DELETED) {
                    return entry;
                }
            }
        }

        current_cluster = fat12_image_get_fat_entry(image, bpb, current_cluster);
    }

    return 0;
}

static uint32_t fat12_image_alloc_cluster(uint8_t* image, uint32_t image_size, const fat12_bpb_t* bpb) {
    uint32_t total_clusters = fat12_image_total_clusters(bpb, image);
    uint32_t cluster;

    for (cluster = 2u; cluster < (total_clusters + 2u); ++cluster) {
        if (fat12_image_get_fat_entry(image, bpb, cluster) == FAT12_CLUSTER_FREE) {
            uint32_t lba = fat12_image_cluster_to_lba(bpb, cluster);
            uint32_t sector_idx;

            fat12_image_set_fat_entry(image, bpb, cluster, FAT12_CLUSTER_EOC);
            for (sector_idx = 0u; sector_idx < bpb->sectors_per_cluster; ++sector_idx) {
                uint8_t* sector = fat12_image_sector(image, image_size, lba + sector_idx);
                uint32_t i;
                if (sector == 0) {
                    return 0u;
                }
                for (i = 0u; i < 512u; ++i) {
                    sector[i] = 0u;
                }
            }
            return cluster;
        }
    }

    return 0u;
}

static void fat12_image_write_dir_entry(uint8_t* entry, const uint8_t name83[11], uint8_t attr, uint16_t cluster, uint32_t size) {
    uint32_t i;

    for (i = 0u; i < 32u; ++i) {
        entry[i] = 0u;
    }
    for (i = 0u; i < 11u; ++i) {
        entry[i] = name83[i];
    }
    entry[11] = attr;
    fat12_put_le16(entry + 26u, cluster);
    entry[28] = (uint8_t)(size & 0xFFu);
    entry[29] = (uint8_t)((size >> 8) & 0xFFu);
    entry[30] = (uint8_t)((size >> 16) & 0xFFu);
    entry[31] = (uint8_t)((size >> 24) & 0xFFu);
}

int fat12_core_create_dir_in_image(uint8_t* image, uint32_t image_size, const char* path) {
    fat12_bpb_t bpb;
    char parent_path[FS_PATH_CAP];
    char leaf_name[FS_NAME_CAP];
    uint8_t leaf83[11];
    uint8_t dot83[11] = {'.', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    uint8_t dotdot83[11] = {'.', '.', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    uint32_t parent_cluster = 0u;
    uint32_t new_cluster;
    uint8_t* parent_entry;
    uint8_t* new_dir;
    uint8_t attr = 0u;
    int rc;

    if (image == 0 || image_size < 512u) {
        return FS_ERR_INVALID;
    }

    rc = fat12_parse_bpb(image, &bpb);
    if (rc != FS_OK) {
        return rc;
    }

    rc = fat12_split_create_path(path, parent_path, leaf_name);
    if (rc != FS_OK) {
        return rc;
    }

    rc = fat12_make_83_dir_name(leaf_name, leaf83);
    if (rc != FS_OK) {
        return rc;
    }

    rc = fat12_image_resolve_dir(image, image_size, &bpb, parent_path, &parent_cluster);
    if (rc != FS_OK) {
        return rc;
    }

    rc = fat12_image_find_in_dir(image, image_size, &bpb, parent_cluster, leaf83, 0, &attr);
    if (rc == FS_OK) {
        return ((attr & FAT12_ATTR_DIRECTORY) != 0u) ? FS_OK : FS_ERR_EXISTS;
    }
    if (rc != FS_ERR_NOT_FOUND) {
        return rc;
    }

    parent_entry = fat12_image_find_free_entry(image, image_size, &bpb, parent_cluster);
    if (parent_entry == 0) {
        return FS_ERR_NO_SPACE;
    }

    new_cluster = fat12_image_alloc_cluster(image, image_size, &bpb);
    if (new_cluster == 0u) {
        return FS_ERR_NO_SPACE;
    }

    fat12_image_write_dir_entry(parent_entry, leaf83, FAT12_ATTR_DIRECTORY, (uint16_t)new_cluster, 0u);

    new_dir = fat12_image_sector(image, image_size, fat12_image_cluster_to_lba(&bpb, new_cluster));
    if (new_dir == 0) {
        return FS_ERR_INVALID;
    }
    fat12_image_write_dir_entry(new_dir, dot83, FAT12_ATTR_DIRECTORY, (uint16_t)new_cluster, 0u);
    fat12_image_write_dir_entry(new_dir + 32u, dotdot83, FAT12_ATTR_DIRECTORY, (uint16_t)parent_cluster, 0u);

    return FS_OK;
}

int fat12_core_mount(block_device_t* dev) {
    int rc;

    rc = block_core_read(dev, 0u, 1u, g_fat12_sector);
    if (rc != FS_OK) {
        return rc;
    }

    rc = fat12_parse_bpb(g_fat12_sector, &g_fat12);
    if (rc != FS_OK) {
        return rc;
    }

    g_fat12_dev = dev;
    g_fat12_cwd[0] = '/';
    g_fat12_cwd[1] = '\0';
    g_fat12_cwd_cluster = 0u;
    return FS_OK;
}

static int fat12_init(void) {
    return (g_fat12_dev == 0) ? FS_ERR_INVALID : FS_OK;
}

static const char* fat12_get_cwd_path(void) {
    return g_fat12_cwd;
}

static uint32_t fat12_cluster_to_lba(uint32_t cluster) {
    if (cluster < 2u) {
        return g_fat12.root_dir_lba;
    }
    return g_fat12.root_dir_lba + g_fat12.root_dir_sectors + (cluster - 2u) * (uint32_t)g_fat12.sectors_per_cluster;
}

static uint32_t fat12_get_next_cluster(uint32_t cluster) {
    uint32_t offset = cluster + (cluster / 2u);
    uint32_t sector = (uint32_t)g_fat12.reserved_sectors + (offset / 512u);
    uint32_t byte_offset = offset % 512u;
    uint8_t sec_buf[512];
    int rc;

    rc = block_core_read(g_fat12_dev, sector, 1u, sec_buf);
    if (rc != FS_OK) {
        return 0xFFFu;
    }

    uint16_t val;
    if (byte_offset == 511u) {
        val = sec_buf[511u];
        rc = block_core_read(g_fat12_dev, sector + 1u, 1u, sec_buf);
        if (rc != FS_OK) {
            return 0xFFFu;
        }
        val |= ((uint16_t)sec_buf[0] << 8);
    } else {
        val = sec_buf[byte_offset] | ((uint16_t)sec_buf[byte_offset + 1u] << 8);
    }

    if ((cluster & 1u) != 0u) {
        val >>= 4;
    } else {
        val &= 0x0FFF;
    }

    return val;
}

static int fat12_find_entry_in_dir(uint32_t dir_cluster, const char* name, uint32_t* out_cluster, uint8_t* out_attr) {
    uint32_t current_cluster = dir_cluster;
    char entry_name[FS_NAME_CAP];
    int rc;

    if (dir_cluster == 0u) {
        uint32_t sector_index;
        uint32_t entry_index;

        for (sector_index = 0u; sector_index < g_fat12.root_dir_sectors; ++sector_index) {
            rc = block_core_read(g_fat12_dev, g_fat12.root_dir_lba + sector_index, 1u, g_fat12_sector);
            if (rc != FS_OK) {
                return rc;
            }

            for (entry_index = 0u; entry_index < 16u; ++entry_index) {
                const uint8_t* entry = g_fat12_sector + (entry_index * 32u);
                uint8_t first = entry[0];
                uint8_t attr = entry[11];

                if (first == FAT12_ENTRY_FREE) {
                    return FS_ERR_NOT_FOUND;
                }

                if (first == FAT12_ENTRY_DELETED || (attr & FAT12_ATTR_VOLUME_ID) != 0u) {
                    continue;
                }

                fat12_copy_83_name(entry_name, entry);
                if (fat12_name_eq(entry_name, name) != 0) {
                    uint16_t start_clust = fat12_le16(entry + 26u);
                    if (out_cluster) *out_cluster = start_clust;
                    if (out_attr) *out_attr = attr;
                    return FS_OK;
                }
            }
        }
        return FS_ERR_NOT_FOUND;
    } else {
        while (current_cluster < 0xFF8u) {
            uint32_t sector_idx;
            uint32_t base_lba = fat12_cluster_to_lba(current_cluster);

            for (sector_idx = 0u; sector_idx < g_fat12.sectors_per_cluster; ++sector_idx) {
                rc = block_core_read(g_fat12_dev, base_lba + sector_idx, 1u, g_fat12_sector);
                if (rc != FS_OK) {
                    return rc;
                }

                uint32_t entry_index;
                for (entry_index = 0u; entry_index < 16u; ++entry_index) {
                    const uint8_t* entry = g_fat12_sector + (entry_index * 32u);
                    uint8_t first = entry[0];
                    uint8_t attr = entry[11];

                    if (first == FAT12_ENTRY_FREE) {
                        return FS_ERR_NOT_FOUND;
                    }

                    if (first == FAT12_ENTRY_DELETED || (attr & FAT12_ATTR_VOLUME_ID) != 0u) {
                        continue;
                    }

                    fat12_copy_83_name(entry_name, entry);
                    if (fat12_name_eq(entry_name, name) != 0) {
                        uint16_t start_clust = fat12_le16(entry + 26u);
                        if (out_cluster) *out_cluster = start_clust;
                        if (out_attr) *out_attr = attr;
                        return FS_OK;
                    }
                }
            }
            current_cluster = fat12_get_next_cluster(current_cluster);
        }
        return FS_ERR_NOT_FOUND;
    }
}

static int fat12_resolve_path(const char* path, uint32_t* out_cluster, uint8_t* out_is_dir) {
    uint8_t current_is_dir = 1u;

    if (path == 0 || path[0] == '\0') {
        return FS_ERR_INVALID;
    }

    uint32_t current_cluster = g_fat12_cwd_cluster;
    const char* cursor = path;

    if (path[0] == '/') {
        current_cluster = 0u;
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

        if (len == 0u || fat12_streq(component, ".") != 0) {
            continue;
        }

        if (fat12_streq(component, "..") != 0 && current_cluster == 0u) {
            continue;
        }

        uint32_t next_cluster = 0u;
        uint8_t attr = 0u;
        int rc = fat12_find_entry_in_dir(current_cluster, component, &next_cluster, &attr);
        if (rc != FS_OK) {
            return rc;
        }

        if (*cursor != '\0') {
            if ((attr & FAT12_ATTR_DIRECTORY) == 0u) {
                return FS_ERR_NOT_DIR;
            }
        }

        current_cluster = next_cluster;
        current_is_dir = ((attr & FAT12_ATTR_DIRECTORY) != 0u) ? 1u : 0u;
        if (out_is_dir) {
            *out_is_dir = current_is_dir;
        }
    }

    if (out_cluster) {
        *out_cluster = current_cluster;
    }
    if (out_is_dir) {
        *out_is_dir = current_is_dir;
    }

    return FS_OK;
}

static void fat12_normalize_path(char* dst, const char* path) {
    char temp[FS_PATH_CAP];

    if (path[0] == '/') {
        temp[0] = '\0';
    } else {
        fat12_copy_string(temp, FS_PATH_CAP, g_fat12_cwd);
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

        if (comp_len == 0u || fat12_streq(comp, ".") != 0) {
            continue;
        }

        if (fat12_streq(comp, "..") != 0) {
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
        fat12_copy_string(dst, FS_PATH_CAP, temp);
    }
}

static int fat12_change_dir(const char* path) {
    if (path == 0 || path[0] == '\0') {
        return FS_ERR_INVALID;
    }

    uint32_t target_cluster = 0u;
    uint8_t is_dir = 0u;
    int rc = fat12_resolve_path(path, &target_cluster, &is_dir);
    if (rc != FS_OK) {
        return rc;
    }

    if (is_dir == 0u) {
        return FS_ERR_NOT_DIR;
    }

    g_fat12_cwd_cluster = target_cluster;
    
    char new_path[FS_PATH_CAP];
    fat12_normalize_path(new_path, path);
    fat12_copy_string(g_fat12_cwd, FS_PATH_CAP, new_path);

    return FS_OK;
}

static int fat12_make_dir(const char* path) {
    (void)path;
    return FS_ERR_READ_ONLY;
}

static int fat12_list_dir(const char* path, fs_dirent_t* entries, uint32_t cap, uint32_t* out_count) {
    uint32_t count = 0u;
    int rc;

    if (entries == 0 || out_count == 0) {
        return FS_ERR_INVALID;
    }

    uint32_t target_cluster = g_fat12_cwd_cluster;
    if (path != 0 && path[0] != '\0' && fat12_streq(path, ".") == 0) {
        uint8_t is_dir = 0u;
        rc = fat12_resolve_path(path, &target_cluster, &is_dir);
        if (rc != FS_OK) {
            return rc;
        }
        if (!is_dir) {
            return FS_ERR_NOT_DIR;
        }
    }

    fat12_clear_names();

    if (target_cluster == 0u) {
        uint32_t sector_index;
        uint32_t entry_index;

        for (sector_index = 0u; sector_index < g_fat12.root_dir_sectors; ++sector_index) {
            rc = block_core_read(g_fat12_dev, g_fat12.root_dir_lba + sector_index, 1u, g_fat12_sector);
            if (rc != FS_OK) {
                return rc;
            }

            for (entry_index = 0u; entry_index < 16u; ++entry_index) {
                const uint8_t* entry = g_fat12_sector + (entry_index * 32u);
                uint8_t first = entry[0];
                uint8_t attr = entry[11];

                if (first == FAT12_ENTRY_FREE) {
                    *out_count = count;
                    return FS_OK;
                }

                if (first == FAT12_ENTRY_DELETED || (attr & FAT12_ATTR_VOLUME_ID) != 0u) {
                    continue;
                }

                if (count >= cap || count >= FAT12_NAME_SLOTS) {
                    *out_count = count;
                    return FS_ERR_NO_SPACE;
                }

                fat12_copy_83_name(g_fat12_names[count], entry);
                entries[count].name = g_fat12_names[count];
                entries[count].type = ((attr & FAT12_ATTR_DIRECTORY) != 0u) ? FS_NODE_DIR : FS_NODE_FILE;
                ++count;
            }
        }
    } else {
        uint32_t current_cluster = target_cluster;
        while (current_cluster < 0xFF8u) {
            uint32_t sector_idx;
            uint32_t base_lba = fat12_cluster_to_lba(current_cluster);

            for (sector_idx = 0u; sector_idx < g_fat12.sectors_per_cluster; ++sector_idx) {
                rc = block_core_read(g_fat12_dev, base_lba + sector_idx, 1u, g_fat12_sector);
                if (rc != FS_OK) {
                    return rc;
                }

                uint32_t entry_index;
                for (entry_index = 0u; entry_index < 16u; ++entry_index) {
                    const uint8_t* entry = g_fat12_sector + (entry_index * 32u);
                    uint8_t first = entry[0];
                    uint8_t attr = entry[11];

                    if (first == FAT12_ENTRY_FREE) {
                        *out_count = count;
                        return FS_OK;
                    }

                    if (first == FAT12_ENTRY_DELETED || (attr & FAT12_ATTR_VOLUME_ID) != 0u) {
                        continue;
                    }

                    if (count >= cap || count >= FAT12_NAME_SLOTS) {
                        *out_count = count;
                        return FS_ERR_NO_SPACE;
                    }

                    fat12_copy_83_name(g_fat12_names[count], entry);
                    entries[count].name = g_fat12_names[count];
                    entries[count].type = ((attr & FAT12_ATTR_DIRECTORY) != 0u) ? FS_NODE_DIR : FS_NODE_FILE;
                    ++count;
                }
            }
            current_cluster = fat12_get_next_cluster(current_cluster);
        }
    }

    *out_count = count;
    return FS_OK;
}

static const vfs_driver_t g_fat12_driver = {
    fat12_init,
    fat12_get_cwd_path,
    fat12_change_dir,
    fat12_make_dir,
    fat12_list_dir
};

const vfs_driver_t* fat12_core_driver(void) {
    return &g_fat12_driver;
}
