#include "fs_core.h"
#include "block_core.h"
#include "fat12_core.h"
#include "iso9660_core.h"
#include "vfs_core.h"

static const vfs_driver_t* g_root_driver = 0;
static uint32_t g_boot_magic = 0u;
static uint32_t g_boot_info_ptr = 0u;

#define FS_FLOPPY_MAGIC 0x53314D47u
#define FS_MB2_BOOTLOADER_MAGIC 0x36D76289u
#define FS_MB2_TAG_TYPE_END 0u
#define FS_MB2_TAG_TYPE_MODULE 3u

typedef struct {
    uint32_t boot_drive;
    uint32_t kernel_load_addr;
    uint32_t kernel_size_bytes;
    uint32_t boot_sector_addr;
    uint32_t fat_addr;
    uint32_t root_dir_addr;
    uint32_t fat_lba;
    uint32_t fat_sectors;
    uint32_t root_lba;
    uint32_t root_sectors;
    uint32_t floppy_image_addr;
} fs_floppy_boot_info_t;

typedef struct {
    uint32_t type;
    uint32_t size;
} fs_mb2_tag_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
} fs_mb2_module_tag_t;

static block_device_t g_boot_floppy_device;
static fs_floppy_boot_info_t* g_boot_floppy_info = 0;
static block_device_t g_mb2_module_device;
static uint32_t g_mb2_module_start = 0u;
static uint32_t g_mb2_module_size = 0u;

static void fs_copy_bytes(uint8_t* dst, const uint8_t* src, uint32_t len) {
    uint32_t i;

    for (i = 0u; i < len; ++i) {
        dst[i] = src[i];
    }
}

static int fs_cached_floppy_read(block_device_t* dev, uint32_t lba, uint32_t count, void* buffer) {
    uint32_t i;
    uint8_t* out = (uint8_t*)buffer;
    fs_floppy_boot_info_t* info = (fs_floppy_boot_info_t*)dev->ctx;

    if (info == 0 || buffer == 0) {
        return FS_ERR_INVALID;
    }

    if (info->floppy_image_addr != 0) {
        fs_copy_bytes((uint8_t*)buffer, (const uint8_t*)((uintptr_t)info->floppy_image_addr + lba * 512u), count * 512u);
        return FS_OK;
    }

    for (i = 0u; i < count; ++i) {
        uint32_t current_lba = lba + i;
        uint8_t* dst = out + (i * 512u);

        if (current_lba == 0u) {
            fs_copy_bytes(dst, (const uint8_t*)(uintptr_t)info->boot_sector_addr, 512u);
            continue;
        }

        if (current_lba >= info->fat_lba && current_lba < (info->fat_lba + info->fat_sectors)) {
            uint32_t offset = (current_lba - info->fat_lba) * 512u;
            fs_copy_bytes(dst, (const uint8_t*)((uintptr_t)info->fat_addr + offset), 512u);
            continue;
        }

        if (current_lba >= info->root_lba && current_lba < (info->root_lba + info->root_sectors)) {
            uint32_t offset = (current_lba - info->root_lba) * 512u;
            fs_copy_bytes(dst, (const uint8_t*)((uintptr_t)info->root_dir_addr + offset), 512u);
            continue;
        }

        return FS_ERR_NOT_FOUND;
    }

    return FS_OK;
}

static int fs_memory_module_read(block_device_t* dev, uint32_t lba, uint32_t count, void* buffer) {
    uint32_t offset;
    uint32_t len;
    (void)dev;

    if (buffer == 0 || count == 0u) {
        return FS_ERR_INVALID;
    }

    offset = lba * 512u;
    len = count * 512u;
    if (offset >= g_mb2_module_size || len > (g_mb2_module_size - offset)) {
        return FS_ERR_INVALID;
    }

    fs_copy_bytes((uint8_t*)buffer, (const uint8_t*)((uintptr_t)g_mb2_module_start + offset), len);
    return FS_OK;
}

static int fs_install_mb2_module_device(void) {
    uint32_t cursor;
    uint32_t total_size;

    if (g_boot_magic != FS_MB2_BOOTLOADER_MAGIC || g_boot_info_ptr == 0u) {
        return FS_ERR_INVALID;
    }

    total_size = *(const uint32_t*)(uintptr_t)g_boot_info_ptr;
    cursor = g_boot_info_ptr + 8u;

    while (cursor < (g_boot_info_ptr + total_size)) {
        const fs_mb2_tag_t* tag = (const fs_mb2_tag_t*)(uintptr_t)cursor;
        uint32_t next;

        if (tag->type == FS_MB2_TAG_TYPE_END) {
            break;
        }

        if (tag->type == FS_MB2_TAG_TYPE_MODULE && tag->size >= sizeof(fs_mb2_module_tag_t)) {
            const fs_mb2_module_tag_t* module = (const fs_mb2_module_tag_t*)tag;

            if (module->mod_end > module->mod_start) {
                g_mb2_module_start = module->mod_start;
                g_mb2_module_size = module->mod_end - module->mod_start;
                g_mb2_module_device.sector_size = 512u;
                g_mb2_module_device.sector_count = g_mb2_module_size / 512u;
                g_mb2_module_device.ctx = 0;
                g_mb2_module_device.read = fs_memory_module_read;
                block_core_set_root_device(&g_mb2_module_device);
                return FS_OK;
            }
        }

        next = cursor + tag->size;
        next = (next + 7u) & ~7u;
        if (next <= cursor) {
            break;
        }
        cursor = next;
    }

    return FS_ERR_NOT_FOUND;
}

static void fs_install_bootmedia_device(void) {
    if (fs_install_mb2_module_device() == FS_OK) {
        return;
    }

    if (g_boot_magic != FS_FLOPPY_MAGIC || g_boot_info_ptr == 0u) {
        return;
    }

    g_boot_floppy_info = (fs_floppy_boot_info_t*)(uintptr_t)g_boot_info_ptr;
    if (g_boot_floppy_info->boot_sector_addr == 0u ||
        g_boot_floppy_info->fat_addr == 0u ||
        g_boot_floppy_info->root_dir_addr == 0u ||
        g_boot_floppy_info->fat_sectors == 0u ||
        g_boot_floppy_info->root_sectors == 0u) {
        return;
    }

    g_boot_floppy_device.sector_size = 512u;
    g_boot_floppy_device.sector_count = 2880u;
    g_boot_floppy_device.ctx = g_boot_floppy_info;
    g_boot_floppy_device.read = fs_cached_floppy_read;
    block_core_set_root_device(&g_boot_floppy_device);
}

static int fs_mount_driver(const vfs_driver_t* driver) {
    int rc;

    if (driver == 0 || driver->init == 0) {
        return FS_ERR_INVALID;
    }

    rc = driver->init();
    if (rc != FS_OK) {
        return rc;
    }

    g_root_driver = driver;
    return FS_OK;
}

static int fs_probe_block_root(block_device_t* dev) {
    int rc;

    if (dev == 0) {
        return FS_ERR_INVALID;
    }

    rc = fat12_core_mount(dev);
    if (rc == FS_OK) {
        return fs_mount_driver(fat12_core_driver());
    }

    rc = iso9660_core_mount(dev);
    if (rc == FS_OK) {
        return fs_mount_driver(iso9660_core_driver());
    }

    return FS_ERR_INVALID;
}

int fs_core_init(void) {
    int rc;
    block_device_t* root_device;

    g_root_driver = 0;
    fs_install_bootmedia_device();
    root_device = block_core_get_root_device();
    rc = fs_probe_block_root(root_device);
    if (rc == FS_OK) {
        return FS_OK;
    }

    return FS_ERR_NOT_FOUND;
}

void fs_core_set_boot_context(uint32_t boot_magic, uint32_t boot_info_ptr) {
    g_boot_magic = boot_magic;
    g_boot_info_ptr = boot_info_ptr;
    (void)g_boot_magic;
    (void)g_boot_info_ptr;
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
