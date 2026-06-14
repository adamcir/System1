#include "fs_core.h"
#include "block_core.h"
#include "fat12_core.h"
#include "iso9660_core.h"
#include "posix.h"
#include "process_core.h"
#include "ramfs_core.h"
#include "vfs_core.h"

static const vfs_driver_t* g_root_driver = 0;
static const vfs_driver_t* g_media_driver = 0;
static uint32_t g_boot_magic = 0u;
static uint32_t g_boot_info_ptr = 0u;

#define FS_FLOPPY_MAGIC 0x53314D47u
#define FS_MB2_BOOTLOADER_MAGIC 0x36D76289u
#define FS_MB2_TAG_TYPE_END 0u
#define FS_MB2_TAG_TYPE_MODULE 3u
#define FS_IMPORT_ENTRY_CAP 32u
#define FS_DIRTY_DIR_CAP 16u
#define FS_DIRTY_FILE_CAP 16u
#define FS_WRITEBACK_FILE_CAP 4096u
#define FS_MEDIA_NODE_FLAG 0x80000000u
#define FS_NODE_ID_MASK 0x7FFFFFFFu

typedef enum {
    FS_MEDIA_NONE = 0,
    FS_MEDIA_FAT12 = 1,
    FS_MEDIA_ISO9660 = 2
} fs_media_kind_t;

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
static fs_media_kind_t g_media_kind = FS_MEDIA_NONE;
static char g_dirty_dirs[FS_DIRTY_DIR_CAP][FS_PATH_CAP];
static char g_dirty_files[FS_DIRTY_FILE_CAP][FS_PATH_CAP];
static uint32_t g_dirty_dir_count = 0u;
static uint32_t g_dirty_file_count = 0u;
static char g_writeback_file[FS_WRITEBACK_FILE_CAP];

int fs_core_to_errno(int rc) {
    if (rc == FS_OK) {
        return POSIX_OK;
    }

    if (rc == FS_ERR_NOT_FOUND) {
        return POSIX_ENOENT;
    }

    if (rc == FS_ERR_EXISTS) {
        return POSIX_EEXIST;
    }

    if (rc == FS_ERR_NOT_DIR) {
        return POSIX_ENOTDIR;
    }

    if (rc == FS_ERR_INVALID) {
        return POSIX_EINVAL;
    }

    if (rc == FS_ERR_NO_SPACE) {
        return POSIX_ENOSPC;
    }

    if (rc == FS_ERR_READ_ONLY) {
        return POSIX_EROFS;
    }

    if (rc == FS_ERR_IS_DIR) {
        return POSIX_EISDIR;
    }

    return POSIX_EIO;
}

static uint8_t fs_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void fs_outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void fs_io_delay(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

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

#define FDC_DOR 0x3F2u
#define FDC_MSR 0x3F4u
#define FDC_FIFO 0x3F5u
#define FDC_CCR 0x3F7u
#define FDC_SECTORS_PER_TRACK 18u
#define FDC_HEADS 2u

static int fs_fdc_wait_send(void) {
    uint32_t i;

    for (i = 0u; i < 1000000u; ++i) {
        uint8_t msr = fs_inb(FDC_MSR);
        if ((msr & 0x80u) != 0u && (msr & 0x40u) == 0u) {
            return FS_OK;
        }
    }

    return FS_ERR_READ_ONLY;
}

static int fs_fdc_wait_recv(void) {
    uint32_t i;

    for (i = 0u; i < 1000000u; ++i) {
        uint8_t msr = fs_inb(FDC_MSR);
        if ((msr & 0x80u) != 0u && (msr & 0x40u) != 0u) {
            return FS_OK;
        }
    }

    return FS_ERR_READ_ONLY;
}

static int fs_fdc_send(uint8_t value) {
    int rc = fs_fdc_wait_send();
    if (rc != FS_OK) {
        return rc;
    }
    fs_outb(FDC_FIFO, value);
    return FS_OK;
}

static int fs_fdc_recv(uint8_t* value) {
    int rc;

    if (value == 0) {
        return FS_ERR_INVALID;
    }

    rc = fs_fdc_wait_recv();
    if (rc != FS_OK) {
        return rc;
    }

    *value = fs_inb(FDC_FIFO);
    return FS_OK;
}

static int fs_fdc_sense_interrupt(uint8_t* st0, uint8_t* cyl) {
    int rc;

    rc = fs_fdc_send(0x08u);
    if (rc != FS_OK) {
        return rc;
    }
    rc = fs_fdc_recv(st0);
    if (rc != FS_OK) {
        return rc;
    }
    return fs_fdc_recv(cyl);
}

static int fs_fdc_reset(void) {
    uint32_t i;
    uint8_t st0 = 0u;
    uint8_t cyl = 0u;
    int rc;

    fs_outb(FDC_DOR, 0x00u);
    for (i = 0u; i < 10000u; ++i) {
        fs_io_delay();
    }
    fs_outb(FDC_DOR, 0x1Cu);
    fs_outb(FDC_CCR, 0x00u);
    for (i = 0u; i < 10000u; ++i) {
        fs_io_delay();
    }

    for (i = 0u; i < 4u; ++i) {
        (void)fs_fdc_sense_interrupt(&st0, &cyl);
    }

    rc = fs_fdc_send(0x03u);
    if (rc != FS_OK) {
        return rc;
    }
    rc = fs_fdc_send(0xDFu);
    if (rc != FS_OK) {
        return rc;
    }
    return fs_fdc_send(0x02u);
}

static int fs_fdc_recalibrate(void) {
    uint32_t i;
    int rc;
    uint8_t st0 = 0u;
    uint8_t cyl = 0u;

    rc = fs_fdc_send(0x07u);
    if (rc != FS_OK) {
        return rc;
    }
    rc = fs_fdc_send(0x00u);
    if (rc != FS_OK) {
        return rc;
    }

    for (i = 0u; i < 100000u; ++i) {
        rc = fs_fdc_sense_interrupt(&st0, &cyl);
        if (rc == FS_OK && (st0 & 0x20u) != 0u) {
            return (cyl == 0u) ? FS_OK : FS_ERR_READ_ONLY;
        }
    }

    return FS_ERR_READ_ONLY;
}

static int fs_fdc_seek(uint8_t cylinder, uint8_t head) {
    uint32_t i;
    int rc;
    uint8_t st0 = 0u;
    uint8_t cyl = 0u;

    rc = fs_fdc_send(0x0Fu);
    if (rc != FS_OK) {
        return rc;
    }
    rc = fs_fdc_send((uint8_t)((head << 2) | 0u));
    if (rc != FS_OK) {
        return rc;
    }
    rc = fs_fdc_send(cylinder);
    if (rc != FS_OK) {
        return rc;
    }

    for (i = 0u; i < 100000u; ++i) {
        rc = fs_fdc_sense_interrupt(&st0, &cyl);
        if (rc == FS_OK && (st0 & 0x20u) != 0u) {
            return (cyl == cylinder) ? FS_OK : FS_ERR_READ_ONLY;
        }
    }

    return FS_ERR_READ_ONLY;
}

static void fs_dma2_setup_write(uint32_t addr, uint16_t count) {
    fs_outb(0x0Au, 0x06u);
    fs_outb(0x0Cu, 0xFFu);
    fs_outb(0x04u, (uint8_t)(addr & 0xFFu));
    fs_outb(0x04u, (uint8_t)((addr >> 8) & 0xFFu));
    fs_outb(0x81u, (uint8_t)((addr >> 16) & 0xFFu));
    fs_outb(0x0Cu, 0xFFu);
    fs_outb(0x05u, (uint8_t)(count & 0xFFu));
    fs_outb(0x05u, (uint8_t)((count >> 8) & 0xFFu));
    fs_outb(0x0Bu, 0x4Au);
    fs_outb(0x0Au, 0x02u);
}

static int fs_fdc_write_sector(uint8_t* image, uint32_t lba) {
    uint32_t track_size = FDC_SECTORS_PER_TRACK * FDC_HEADS;
    uint8_t cylinder = (uint8_t)(lba / track_size);
    uint8_t temp = (uint8_t)(lba % track_size);
    uint8_t head = (uint8_t)(temp / FDC_SECTORS_PER_TRACK);
    uint8_t sector = (uint8_t)((temp % FDC_SECTORS_PER_TRACK) + 1u);
    uint32_t addr = (uint32_t)(uintptr_t)(image + lba * 512u);
    uint8_t result[7];
    uint32_t i;
    int rc;

    if (((addr & 0xFFFFu) + 511u) > 0xFFFFu) {
        return FS_ERR_INVALID;
    }

    rc = fs_fdc_seek(cylinder, head);
    if (rc != FS_OK) {
        return rc;
    }

    fs_dma2_setup_write(addr, 511u);

    rc = fs_fdc_send(0x45u);
    if (rc != FS_OK) {
        return rc;
    }
    rc = fs_fdc_send((uint8_t)((head << 2) | 0u));
    if (rc != FS_OK) {
        return rc;
    }
    rc = fs_fdc_send(cylinder);
    if (rc != FS_OK) {
        return rc;
    }
    rc = fs_fdc_send(head);
    if (rc != FS_OK) {
        return rc;
    }
    rc = fs_fdc_send(sector);
    if (rc != FS_OK) {
        return rc;
    }
    rc = fs_fdc_send(0x02u);
    if (rc != FS_OK) {
        return rc;
    }
    rc = fs_fdc_send(FDC_SECTORS_PER_TRACK);
    if (rc != FS_OK) {
        return rc;
    }
    rc = fs_fdc_send(0x1Bu);
    if (rc != FS_OK) {
        return rc;
    }
    rc = fs_fdc_send(0xFFu);
    if (rc != FS_OK) {
        return rc;
    }

    for (i = 0u; i < 7u; ++i) {
        rc = fs_fdc_recv(&result[i]);
        if (rc != FS_OK) {
            return rc;
        }
    }

    if ((result[0] & 0xC0u) != 0u || result[1] != 0u || result[2] != 0u) {
        return FS_ERR_READ_ONLY;
    }

    return FS_OK;
}

static int fs_fdc_write_image(uint8_t* image, uint32_t sector_count) {
    uint32_t lba;
    int rc;

    if (image == 0 || sector_count == 0u) {
        return FS_ERR_INVALID;
    }

    rc = fs_fdc_reset();
    if (rc != FS_OK) {
        return rc;
    }
    rc = fs_fdc_recalibrate();
    if (rc != FS_OK) {
        return rc;
    }

    for (lba = 0u; lba < sector_count; ++lba) {
        rc = fs_fdc_write_sector(image, lba);
        if (rc != FS_OK) {
            return rc;
        }
    }

    fs_outb(FDC_DOR, 0x0Cu);
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

    g_media_kind = FS_MEDIA_NONE;
    rc = fat12_core_mount(dev);
    if (rc == FS_OK) {
        g_media_kind = FS_MEDIA_FAT12;
        return fs_mount_driver(fat12_core_driver());
    }

    rc = iso9660_core_mount(dev);
    if (rc == FS_OK) {
        g_media_kind = FS_MEDIA_ISO9660;
        return fs_mount_driver(iso9660_core_driver());
    }

    return FS_ERR_INVALID;
}

static int fs_path_join(char* out, uint32_t cap, const char* dir, const char* name) {
    uint32_t pos = 0u;
    uint32_t i = 0u;

    if (out == 0 || cap == 0u || dir == 0 || name == 0 || name[0] == '\0') {
        return FS_ERR_INVALID;
    }

    if (dir[0] != '/') {
        return FS_ERR_INVALID;
    }

    while (dir[i] != '\0') {
        if (pos + 1u >= cap) {
            return FS_ERR_INVALID;
        }
        out[pos++] = dir[i++];
    }

    if (pos > 1u && out[pos - 1u] == '/') {
        --pos;
    }

    if (pos + 1u >= cap) {
        return FS_ERR_INVALID;
    }
    out[pos++] = '/';

    i = 0u;
    while (name[i] != '\0') {
        if (pos + 1u >= cap) {
            return FS_ERR_INVALID;
        }
        out[pos++] = name[i++];
    }

    out[pos] = '\0';
    return FS_OK;
}

static void fs_copy_name(char* dst, uint32_t cap, const char* src) {
    uint32_t i = 0u;

    if (dst == 0 || cap == 0u) {
        return;
    }

    if (src == 0) {
        dst[0] = '\0';
        return;
    }

    while (src[i] != '\0' && (i + 1u) < cap) {
        dst[i] = src[i];
        ++i;
    }

    dst[i] = '\0';
}

static int fs_streq_local(const char* a, const char* b) {
    uint32_t i = 0u;

    if (a == 0 || b == 0) {
        return 0;
    }

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        ++i;
    }

    return (int)(a[i] == '\0' && b[i] == '\0');
}

int fs_core_normalize_path(const char* cwd, const char* path, char* out, uint32_t out_cap) {
    char temp[FS_PATH_CAP];
    uint32_t len = 0u;
    const char* cursor;

    if (path == 0 || path[0] == '\0' || out == 0 || out_cap < 2u) {
        return FS_ERR_INVALID;
    }

    if (path[0] == '/') {
        temp[0] = '\0';
        cursor = path;
        while (*cursor == '/') {
            ++cursor;
        }
    } else {
        fs_copy_name(temp, FS_PATH_CAP, (cwd == 0 || cwd[0] == '\0') ? "/" : cwd);
        cursor = path;
    }

    while (temp[len] != '\0') {
        ++len;
    }

    while (*cursor != '\0') {
        char comp[FS_NAME_CAP];
        uint32_t comp_len = 0u;

        while (*cursor != '\0' && *cursor != '/') {
            if (comp_len + 1u >= FS_NAME_CAP) {
                return FS_ERR_INVALID;
            }
            comp[comp_len++] = *cursor++;
        }
        comp[comp_len] = '\0';
        while (*cursor == '/') {
            ++cursor;
        }

        if (comp_len == 0u || fs_streq_local(comp, ".") != 0) {
            continue;
        }

        if (fs_streq_local(comp, "..") != 0) {
            while (len > 1u && temp[len - 1u] != '/') {
                --len;
            }
            if (len > 1u) {
                --len;
            }
            temp[len] = '\0';
            continue;
        }

        if (len == 0u) {
            if (len + 1u >= FS_PATH_CAP) {
                return FS_ERR_INVALID;
            }
            temp[len++] = '/';
        } else if (len == 1u && temp[0] == '/') {
        } else {
            if (len + 1u >= FS_PATH_CAP) {
                return FS_ERR_INVALID;
            }
            temp[len++] = '/';
        }

        for (uint32_t i = 0u; i < comp_len; ++i) {
            if (len + 1u >= FS_PATH_CAP) {
                return FS_ERR_INVALID;
            }
            temp[len++] = comp[i];
        }
        temp[len] = '\0';
    }

    if (temp[0] == '\0') {
        out[0] = '/';
        out[1] = '\0';
    } else {
        if (len + 1u > out_cap) {
            return FS_ERR_INVALID;
        }
        fs_copy_name(out, out_cap, temp);
    }

    return FS_OK;
}

static void fs_dirty_dirs_clear(void) {
    uint32_t i;

    for (i = 0u; i < FS_DIRTY_DIR_CAP; ++i) {
        g_dirty_dirs[i][0] = '\0';
    }
    g_dirty_dir_count = 0u;
}

static void fs_dirty_files_clear(void) {
    uint32_t i;

    for (i = 0u; i < FS_DIRTY_FILE_CAP; ++i) {
        g_dirty_files[i][0] = '\0';
    }
    g_dirty_file_count = 0u;
}

static int fs_record_dirty_dir(const char* path) {
    uint32_t i;

    if (path == 0 || path[0] == '\0') {
        return FS_ERR_INVALID;
    }

    for (i = 0u; i < g_dirty_dir_count; ++i) {
        if (fs_streq_local(g_dirty_dirs[i], path) != 0) {
            return FS_OK;
        }
    }

    if (g_dirty_dir_count >= FS_DIRTY_DIR_CAP) {
        return FS_ERR_NO_SPACE;
    }

    fs_copy_name(g_dirty_dirs[g_dirty_dir_count], FS_PATH_CAP, path);
    ++g_dirty_dir_count;
    return FS_OK;
}

static int fs_record_dirty_file(const char* path) {
    uint32_t i;

    if (path == 0 || path[0] == '\0') {
        return FS_ERR_INVALID;
    }

    for (i = 0u; i < g_dirty_file_count; ++i) {
        if (fs_streq_local(g_dirty_files[i], path) != 0) {
            return FS_OK;
        }
    }

    if (g_dirty_file_count >= FS_DIRTY_FILE_CAP) {
        return FS_ERR_NO_SPACE;
    }

    fs_copy_name(g_dirty_files[g_dirty_file_count], FS_PATH_CAP, path);
    ++g_dirty_file_count;
    return FS_OK;
}

static int fs_is_special_dir_entry(const char* name) {
    if (name == 0) {
        return 0;
    }

    if (name[0] == '.' && name[1] == '\0') {
        return 1;
    }

    if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
        return 1;
    }

    return 0;
}

static int fs_import_media_dir(const char* path) {
    fs_dirent_t entries[FS_IMPORT_ENTRY_CAP];
    char entry_names[FS_IMPORT_ENTRY_CAP][FS_NAME_CAP];
    uint8_t entry_types[FS_IMPORT_ENTRY_CAP];
    uint32_t count = 0u;
    uint32_t i;
    int rc;

    if (g_media_driver == 0 || g_media_driver->list_dir == 0) {
        return FS_ERR_INVALID;
    }

    rc = g_media_driver->list_dir(path, entries, FS_IMPORT_ENTRY_CAP, &count);
    if (rc != FS_OK) {
        return rc;
    }

    for (i = 0u; i < count; ++i) {
        fs_copy_name(entry_names[i], FS_NAME_CAP, entries[i].name);
        entry_types[i] = entries[i].type;
    }

    for (i = 0u; i < count; ++i) {
        char child_path[FS_PATH_CAP];

        if (fs_is_special_dir_entry(entry_names[i]) != 0) {
            continue;
        }

        rc = fs_path_join(child_path, FS_PATH_CAP, path, entry_names[i]);
        if (rc != FS_OK) {
            return rc;
        }

        if (entry_types[i] == FS_NODE_DIR) {
            rc = ramfs_core_import_dir(child_path);
            if (rc != FS_OK) {
                return rc;
            }

            rc = fs_import_media_dir(child_path);
            if (rc != FS_OK) {
                return rc;
            }
            continue;
        }

        if (entry_types[i] == FS_NODE_FILE) {
            rc = ramfs_core_import_file(child_path);
            if (rc != FS_OK) {
                return rc;
            }
            continue;
        }

        return FS_ERR_INVALID;
    }

    return FS_OK;
}

static int fs_switch_root_to_ramfs(void) {
    int rc;

    if (g_root_driver == 0) {
        return FS_ERR_INVALID;
    }

    g_media_driver = g_root_driver;

    rc = ramfs_core_reset_empty();
    if (rc != FS_OK) {
        return rc;
    }

    rc = fs_import_media_dir("/");
    if (rc != FS_OK) {
        return rc;
    }

    ramfs_core_clear_dirty();
    g_root_driver = ramfs_core_driver();
    return FS_OK;
}

int fs_core_init(void) {
    int rc;
    block_device_t* root_device;

    g_root_driver = 0;
    g_media_driver = 0;
    g_media_kind = FS_MEDIA_NONE;
    fs_dirty_dirs_clear();
    fs_dirty_files_clear();
    fs_install_bootmedia_device();
    root_device = block_core_get_root_device();
    rc = fs_probe_block_root(root_device);
    if (rc == FS_OK) {
        return fs_switch_root_to_ramfs();
    }

    return FS_ERR_NOT_FOUND;
}

void fs_core_set_boot_context(uint32_t boot_magic, uint32_t boot_info_ptr) {
    g_boot_magic = boot_magic;
    g_boot_info_ptr = boot_info_ptr;
    (void)g_boot_magic;
    (void)g_boot_info_ptr;
}

uint8_t fs_core_has_pending_changes(void) {
    return ramfs_core_is_dirty();
}

static int fs_core_flush_to_boot_media(void) {
    uint32_t i;
    uint8_t* image;

    if (g_media_kind == FS_MEDIA_ISO9660) {
        return FS_ERR_READ_ONLY;
    }

    if (g_media_kind == FS_MEDIA_FAT12) {
        if (g_boot_floppy_info == 0 || g_boot_floppy_info->floppy_image_addr == 0u) {
            return FS_ERR_READ_ONLY;
        }

        image = (uint8_t*)(uintptr_t)g_boot_floppy_info->floppy_image_addr;
        for (i = 0u; i < g_dirty_dir_count; ++i) {
            int rc = fat12_core_create_dir_in_image(image, 2880u * 512u, g_dirty_dirs[i]);
            if (rc != FS_OK) {
                return rc;
            }
        }

        for (i = 0u; i < g_dirty_file_count; ++i) {
            uint32_t size = 0u;
            int rc = ramfs_core_read_file(g_dirty_files[i], g_writeback_file, FS_WRITEBACK_FILE_CAP, &size);
            if (rc != FS_OK) {
                return rc;
            }

            rc = fat12_core_write_file_in_image(image, 2880u * 512u, g_dirty_files[i], g_writeback_file, size);
            if (rc != FS_OK) {
                return rc;
            }
        }

        {
            int rc = fs_fdc_write_image(image, 2880u);
            if (rc != FS_OK) {
                return rc;
            }
        }

        fs_dirty_dirs_clear();
        fs_dirty_files_clear();
        return FS_OK;
    }

    return FS_ERR_INVALID;
}

int fs_core_shutdown(uint8_t write_changes) {
    int rc;

    if (ramfs_core_is_dirty() == 0u) {
        return FS_OK;
    }

    if (write_changes == 0u) {
        return FS_OK;
    }

    rc = fs_core_flush_to_boot_media();
    if (rc == FS_OK) {
        ramfs_core_clear_dirty();
    }

    return rc;
}

const char* fs_core_get_cwd_path(void) {
    process_t* current = process_core_current();

    if (current != 0 && current->cwd[0] != '\0') {
        return current->cwd;
    }

    if (g_root_driver == 0 || g_root_driver->get_cwd_path == 0) {
        return "/";
    }

    return g_root_driver->get_cwd_path();
}

int fs_core_change_dir(const char* path) {
    char full_path[FS_PATH_CAP];
    int rc;

    if (g_root_driver == 0 || g_root_driver->change_dir == 0) {
        return FS_ERR_INVALID;
    }

    rc = fs_core_normalize_path(fs_core_get_cwd_path(), path, full_path, FS_PATH_CAP);
    if (rc != FS_OK) {
        return rc;
    }

    rc = g_root_driver->change_dir(full_path);
    if (rc != FS_OK) {
        return rc;
    }

    process_core_set_cwd(process_core_current(), full_path);
    return FS_OK;
}

int fs_core_make_dir(const char* path) {
    char full_path[FS_PATH_CAP];
    const char* cwd;
    int rc;

    if (g_root_driver == 0 || g_root_driver->make_dir == 0) {
        return FS_ERR_READ_ONLY;
    }

    cwd = fs_core_get_cwd_path();
    rc = fs_core_normalize_path(cwd, path, full_path, FS_PATH_CAP);
    if (rc != FS_OK) {
        return rc;
    }

    rc = g_root_driver->make_dir(full_path);
    if (rc != FS_OK) {
        return rc;
    }

    if (g_media_kind == FS_MEDIA_FAT12) {
        rc = fs_record_dirty_dir(full_path);
        if (rc != FS_OK) {
            return rc;
        }
    }

    return FS_OK;
}

int fs_core_list_dir(const char* path, fs_dirent_t* entries, uint32_t cap, uint32_t* out_count) {
    char full_path[FS_PATH_CAP];
    int rc;

    if (g_root_driver == 0 || g_root_driver->list_dir == 0) {
        return FS_ERR_INVALID;
    }

    if (path == 0 || path[0] == '\0') {
        path = ".";
    }

    rc = fs_core_normalize_path(fs_core_get_cwd_path(), path, full_path, FS_PATH_CAP);
    if (rc != FS_OK) {
        return rc;
    }

    return g_root_driver->list_dir(full_path, entries, cap, out_count);
}

int fs_core_read_file(const char* path, char* buffer, uint32_t cap, uint32_t* out_size) {
    char full_path[FS_PATH_CAP];
    int rc;

    if (path == 0 || buffer == 0 || out_size == 0) {
        return FS_ERR_INVALID;
    }

    if (g_media_driver != 0 && g_media_driver->read_file != 0) {
        rc = fs_core_normalize_path(fs_core_get_cwd_path(), path, full_path, FS_PATH_CAP);
        if (rc != FS_OK) {
            return rc;
        }

        return g_media_driver->read_file(full_path, buffer, cap, out_size);
    }

    if (g_root_driver != 0 && g_root_driver->read_file != 0) {
        rc = fs_core_normalize_path(fs_core_get_cwd_path(), path, full_path, FS_PATH_CAP);
        if (rc != FS_OK) {
            return rc;
        }

        return g_root_driver->read_file(full_path, buffer, cap, out_size);
    }

    return FS_ERR_INVALID;
}

int fs_core_open(const char* path, uint32_t flags, uint32_t* out_node_id) {
    char full_path[FS_PATH_CAP];
    uint32_t node_id = 0u;
    int rc;

    if (path == 0 || out_node_id == 0) {
        return FS_ERR_INVALID;
    }

    rc = fs_core_normalize_path(fs_core_get_cwd_path(), path, full_path, FS_PATH_CAP);
    if (rc != FS_OK) {
        return rc;
    }

    if ((flags & (FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC | FS_O_APPEND)) == 0u &&
        g_media_driver != 0 && g_media_driver->open != 0) {
        rc = g_media_driver->open(full_path, flags, &node_id);
        if (rc == FS_OK) {
            *out_node_id = node_id | FS_MEDIA_NODE_FLAG;
            return FS_OK;
        }
    }

    if (g_root_driver == 0 || g_root_driver->open == 0) {
        return FS_ERR_INVALID;
    }

    rc = g_root_driver->open(full_path, flags, &node_id);
    if (rc != FS_OK) {
        return rc;
    }

    if (g_media_kind == FS_MEDIA_FAT12 &&
        ((flags & FS_O_CREAT) != 0u ||
         (flags & FS_O_TRUNC) != 0u ||
         (flags & FS_O_APPEND) != 0u ||
         (flags & FS_O_RDWR) == FS_O_WRONLY ||
         (flags & FS_O_RDWR) == FS_O_RDWR)) {
        rc = fs_record_dirty_file(full_path);
        if (rc != FS_OK) {
            return rc;
        }
    }

    *out_node_id = node_id;
    return FS_OK;
}

int fs_core_read(uint32_t node_id, uint32_t offset, char* buffer, uint32_t cap, uint32_t* out_size) {
    const vfs_driver_t* driver;
    uint32_t local_id;

    if ((node_id & FS_MEDIA_NODE_FLAG) != 0u) {
        driver = g_media_driver;
        local_id = node_id & FS_NODE_ID_MASK;
    } else {
        driver = g_root_driver;
        local_id = node_id;
    }

    if (driver == 0 || driver->read == 0) {
        return FS_ERR_INVALID;
    }

    return driver->read(local_id, offset, buffer, cap, out_size);
}

int fs_core_write(uint32_t node_id, uint32_t offset, const char* buffer, uint32_t size, uint32_t* out_written) {
    const vfs_driver_t* driver;
    uint32_t local_id;

    if ((node_id & FS_MEDIA_NODE_FLAG) != 0u) {
        driver = g_media_driver;
        local_id = node_id & FS_NODE_ID_MASK;
    } else {
        driver = g_root_driver;
        local_id = node_id;
    }

    if (driver == 0 || driver->write == 0) {
        return FS_ERR_READ_ONLY;
    }

    return driver->write(local_id, offset, buffer, size, out_written);
}

int fs_core_size(uint32_t node_id, uint32_t* out_size) {
    const vfs_driver_t* driver;
    uint32_t local_id;

    if ((node_id & FS_MEDIA_NODE_FLAG) != 0u) {
        driver = g_media_driver;
        local_id = node_id & FS_NODE_ID_MASK;
    } else {
        driver = g_root_driver;
        local_id = node_id;
    }

    if (driver == 0 || driver->size == 0) {
        return FS_ERR_INVALID;
    }

    return driver->size(local_id, out_size);
}

int fs_core_close(uint32_t node_id) {
    const vfs_driver_t* driver;
    uint32_t local_id;

    if ((node_id & FS_MEDIA_NODE_FLAG) != 0u) {
        driver = g_media_driver;
        local_id = node_id & FS_NODE_ID_MASK;
    } else {
        driver = g_root_driver;
        local_id = node_id;
    }

    if (driver == 0 || driver->close == 0) {
        return FS_OK;
    }

    return driver->close(local_id);
}

int fs_core_stat(const char* path, fs_stat_t* out_stat) {
    char full_path[FS_PATH_CAP];
    int rc;

    if (path == 0 || out_stat == 0) {
        return FS_ERR_INVALID;
    }

    rc = fs_core_normalize_path(fs_core_get_cwd_path(), path, full_path, FS_PATH_CAP);
    if (rc != FS_OK) {
        return rc;
    }

    if (g_media_driver != 0 && g_media_driver->stat != 0) {
        rc = g_media_driver->stat(full_path, out_stat);
        if (rc == FS_OK) {
            return FS_OK;
        }
    }

    if (g_root_driver == 0 || g_root_driver->stat == 0) {
        return FS_ERR_INVALID;
    }

    return g_root_driver->stat(full_path, out_stat);
}

int fs_core_fstat(uint32_t node_id, fs_stat_t* out_stat) {
    const vfs_driver_t* driver;
    uint32_t local_id;

    if ((node_id & FS_MEDIA_NODE_FLAG) != 0u) {
        driver = g_media_driver;
        local_id = node_id & FS_NODE_ID_MASK;
    } else {
        driver = g_root_driver;
        local_id = node_id;
    }

    if (driver == 0 || driver->fstat == 0) {
        return FS_ERR_INVALID;
    }

    return driver->fstat(local_id, out_stat);
}

int fs_core_unlink(const char* path) {
    char full_path[FS_PATH_CAP];
    int rc;

    if (path == 0) {
        return FS_ERR_INVALID;
    }

    rc = fs_core_normalize_path(fs_core_get_cwd_path(), path, full_path, FS_PATH_CAP);
    if (rc != FS_OK) {
        return rc;
    }

    if (g_root_driver == 0 || g_root_driver->unlink == 0) {
        return FS_ERR_READ_ONLY;
    }

    return g_root_driver->unlink(full_path);
}
