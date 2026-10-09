#include "types.h"
#include "smod.h"
#include "module_core.h"
#include "fs_core.h"
#include "mm.h"
#include "klog.h"

#define SMOD_MAX_LOADED 8u
#define SMOD_MAX_DIRECTORY_ENTRIES 24u
#define SMOD_IO_CHUNK 512u
#define SMOD_PATH_PREFIX "/boot/modules/"
#define SMOD_PATH_PREFIX_LENGTH 14u

/* Bootstrap module registry. Modules stay resident until shutdown. */
static void* smod_images[SMOD_MAX_LOADED];
static uint32_t smod_loaded_count;

/* The floppy profile has only 1 MiB RAM. Its legacy MM fallback can hand
 * kmalloc() physical addresses above installed RAM. Execute a small boot
 * module from a resident kernel arena instead of touching those pages. */
#define SMOD_FLOPPY_MAGIC 0x53314D47u
/* Early-boot executable arena: available before kmalloc() and filesystem.
 * Floppy also uses it for any additional small optional modules. */
static uint8_t smod_boot_image[SMOD_IMAGE_LIMIT] __attribute__((aligned(16)));
static uint32_t smod_boot_used;
static char smod_preloaded_names[SMOD_MAX_LOADED][FS_NAME_CAP];
static uint32_t smod_preloaded_count;

/* Modules register services transactionally: failed init leaves no hooks. */
static smod_uart_write_t active_uart;
static smod_rtc_read_t active_rtc;
static smod_uart_write_t pending_uart;
static smod_rtc_read_t pending_rtc;
static uint8_t smod_in_entry;

static int smod_register_uart(smod_uart_write_t write_byte) {
    if (!smod_in_entry || !write_byte || active_uart || pending_uart) return -1;
    pending_uart = write_byte;
    return 0;
}
static int smod_register_rtc(smod_rtc_read_t read_time) {
    if (!smod_in_entry || !read_time || active_rtc || pending_rtc) return -1;
    pending_rtc = read_time;
    return 0;
}
void smod_serial_write(char value) {
    if (active_uart) active_uart(value);
}
int smod_rtc_read(smod_clock_time_t* time) {
    return active_rtc ? active_rtc(time) : -1;
}
static void smod_format_two(char* dest, uint32_t value) {
    dest[0] = (char)('0' + (value / 10u) % 10u);
    dest[1] = (char)('0' + value % 10u);
}
static void smod_report_clock(void) {
    smod_clock_time_t now;
    char buf[] = "0000-00-00 00:00:00";
    if (smod_rtc_read(&now) != 0) return;
    smod_format_two(buf, (uint32_t)now.year / 100u);
    smod_format_two(buf + 2, (uint32_t)now.year % 100u);
    smod_format_two(buf + 5, now.month);
    smod_format_two(buf + 8, now.day);
    smod_format_two(buf + 11, now.hour);
    smod_format_two(buf + 14, now.minute);
    smod_format_two(buf + 17, now.second);
    klog_info("rtc", buf);
}

static uint16_t smod_u16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t smod_u32(const uint8_t* p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint16_t smod_machine_arch(void) {
#if defined(__x86_64__)
    return SMOD_ARCH_X86_64;
#elif defined(__i386__)
    return SMOD_ARCH_I386;
#else
    return 0u;
#endif
}

static int smod_read_exact(uint32_t id, uint32_t offset, uint8_t* data, uint32_t size) {
    uint32_t done = 0u;
    while (done < size) {
        uint32_t got = 0u;
        uint32_t chunk = size - done;
        if (chunk > SMOD_IO_CHUNK) chunk = SMOD_IO_CHUNK;
        if (fs_core_read(id, offset + done, (char*)data + done, chunk, &got) != FS_OK ||
            got == 0u || got > chunk) return -1;
        done += got;
    }
    return 0;
}

static void smod_report_ready(void) {
    klog_info("smod", "Module successfully called System Module API");
}

static const smod_api_v2_t smod_api = {
    SMOD_API_VERSION, smod_report_ready, smod_register_uart, smod_register_rtc
};

/* One header validator for memory-resident and filesystem-backed SMOD. */
static int smod_validate(const uint8_t* hdr, uint32_t bytes,
                         uint32_t* code_size, uint32_t* memory_size,
                         uint32_t* entry_offset) {
    uint32_t code;
    uint32_t mem;
    uint32_t entry;
    if (!hdr || !code_size || !memory_size || !entry_offset ||
        bytes < SMOD_HEADER_SIZE || bytes > SMOD_HEADER_SIZE + SMOD_IMAGE_LIMIT)
        return -1;
    code = smod_u32(hdr + 16u);
    mem = smod_u32(hdr + 20u);
    entry = smod_u32(hdr + 24u);
    if (hdr[0] != 'S' || hdr[1] != 'M' || hdr[2] != 'O' || hdr[3] != 'D' ||
        smod_u16(hdr + 4u) != SMOD_FORMAT_VERSION ||
        smod_u16(hdr + 6u) != SMOD_API_VERSION ||
        smod_u16(hdr + 8u) != smod_machine_arch() ||
        smod_u16(hdr + 10u) != SMOD_FLAG_EXECUTABLE ||
        smod_u32(hdr + 12u) != SMOD_HEADER_SIZE ||
        smod_u32(hdr + 28u) != 0u ||
        code == 0u || code > SMOD_IMAGE_LIMIT ||
        mem < code || mem > SMOD_IMAGE_LIMIT ||
        entry >= code || bytes != SMOD_HEADER_SIZE + code)
        return -1;
    *code_size = code;
    *memory_size = mem;
    *entry_offset = entry;
    return 0;
}

/* Common module activation for both boot stages; registration is atomic. */
static int smod_activate(void* image, uint32_t entry_offset,
                         uint8_t from_arena, uint32_t aligned_size) {
    int rc;
    smod_entry_fn_t entry = (smod_entry_fn_t)((uint8_t*)image + entry_offset);
    pending_uart = 0;
    pending_rtc = 0;
    smod_in_entry = 1u;
    rc = entry(&smod_api);
    smod_in_entry = 0u;
    if (rc != 0) {
        pending_uart = 0;
        pending_rtc = 0;
        return -1;
    }
    if (from_arena) smod_boot_used += aligned_size;
    if (pending_uart) active_uart = pending_uart;
    if (pending_rtc) active_rtc = pending_rtc;
    pending_uart = 0;
    pending_rtc = 0;
    smod_images[smod_loaded_count++] = image;
    return 0;
}

static uint8_t smod_char_lower(uint8_t c) {
    return (c >= 'A' && c <= 'Z') ? (uint8_t)(c + ('a' - 'A')) : c;
}

static int smod_name_already_loaded(const char* name) {
    uint32_t i;
    for (i = 0u; i < smod_preloaded_count; ++i) {
        uint32_t j = 0u;
        while (j < FS_NAME_CAP &&
               smod_char_lower((uint8_t)name[j]) ==
                   smod_char_lower((uint8_t)smod_preloaded_names[i][j]) &&
               name[j] != '\0') ++j;
        if (j < FS_NAME_CAP && name[j] == '\0' &&
            smod_preloaded_names[i][j] == '\0') return 1;
    }
    return 0;
}

/* Bootloader-supplied or compiled-in native SMOD. No FS/MM dependency. */
int smod_core_preload(const uint8_t* file, uint32_t bytes, const char* name) {
    uint32_t image_size, memory_size, entry_offset;
    uint32_t aligned_size, i;
    uint8_t* image;
    if (!file || !name || smod_loaded_count >= SMOD_MAX_LOADED ||
        smod_preloaded_count >= SMOD_MAX_LOADED ||
        smod_name_already_loaded(name) ||
        smod_validate(file, bytes, &image_size, &memory_size, &entry_offset) != 0)
        return -1;
    /* Names are canonicalized and bounded before committing the registration. */
    for (i = 0u; i < FS_NAME_CAP; ++i) if (name[i] == '\0') break;
    if (i == 0u || i == FS_NAME_CAP) return -1;
    aligned_size = (memory_size + 15u) & ~15u;
    if (aligned_size > SMOD_IMAGE_LIMIT - smod_boot_used) return -1;
    image = smod_boot_image + smod_boot_used;
    for (i = 0u; i < image_size; ++i) image[i] = file[SMOD_HEADER_SIZE + i];
    for (; i < memory_size; ++i) image[i] = 0u;
    if (smod_activate(image, entry_offset, 1u, aligned_size) != 0) return -1;
    for (i = 0u; i + 1u < FS_NAME_CAP && name[i] != '\0'; ++i)
        smod_preloaded_names[smod_preloaded_count][i] = name[i];
    smod_preloaded_names[smod_preloaded_count][i] = '\0';
    ++smod_preloaded_count;
    return 0;
}

/* Native v1: code+optional BSS, absolute entry offset and no relocations yet.
 * The .mod image must be self-contained position-independent machine code.
 */
static int smod_core_load_file(const char* path, uint32_t boot_magic) {
    uint8_t hdr[SMOD_HEADER_SIZE];
    fs_stat_t st;
    uint32_t id = 0u;
    uint32_t image_size;
    uint32_t memory_size;
    uint32_t entry_offset;
    uint32_t i;
    void* image;
    int rc;
    uint8_t lowmem_arena = 0u;
    uint32_t aligned_size = 0u;

    if (smod_loaded_count >= SMOD_MAX_LOADED) return -1;
    if (fs_core_open(path, FS_O_RDONLY, &id) != FS_OK) return -1;

    rc = fs_core_fstat(id, &st);
    if (rc != FS_OK || (st.mode & FS_MODE_FILE) == 0u ||
        st.size < SMOD_HEADER_SIZE || st.size > SMOD_HEADER_SIZE + SMOD_IMAGE_LIMIT ||
        smod_read_exact(id, 0u, hdr, SMOD_HEADER_SIZE) != 0) {
        (void)fs_core_close(id);
        return -1;
    }

    if (smod_validate(hdr, st.size, &image_size, &memory_size,
                      &entry_offset) != 0) {
        (void)fs_core_close(id);
        return -1;
    }

    if (boot_magic == SMOD_FLOPPY_MAGIC) {
        aligned_size = (memory_size + 15u) & ~15u;
        if (aligned_size > SMOD_IMAGE_LIMIT - smod_boot_used) {
            (void)fs_core_close(id);
            klog_info("smod", "Boot module arena full");
            return -1;
        }
        image = smod_boot_image + smod_boot_used;
        lowmem_arena = 1u;
    } else {
        image = kmalloc(memory_size);
        if (!image) {
            (void)fs_core_close(id);
            return -1;
        }
    }

    rc = smod_read_exact(id, SMOD_HEADER_SIZE, (uint8_t*)image, image_size);
    (void)fs_core_close(id);
    if (rc != 0) {
        if (!lowmem_arena) kfree(image);
        return -1;
    }
    for (i = image_size; i < memory_size; ++i) ((uint8_t*)image)[i] = 0u;

    rc = smod_activate(image, entry_offset, lowmem_arena, aligned_size);
    if (rc != 0 && !lowmem_arena) kfree(image);
    return rc;
}

static int smod_has_extension(const char* name) {
    uint32_t len = 0u;
    while (len < FS_NAME_CAP && name[len] != '\0') ++len;
    if (len < 5u || len >= FS_NAME_CAP) return 0;
    return name[len-4u] == '.' &&
        (name[len-3u] == 'm' || name[len-3u] == 'M') &&
        (name[len-2u] == 'o' || name[len-2u] == 'O') &&
        (name[len-1u] == 'd' || name[len-1u] == 'D');
}

int smod_core_boot_load_all(uint32_t boot_magic) {
    fs_dirent_t entries[SMOD_MAX_DIRECTORY_ENTRIES];
    uint32_t count = 0u;
    uint32_t i;
    int rc;
    int loaded = 0;

    klog_info("smod", "Scanning /boot/modules");
    rc = fs_core_list_dir("/boot/modules", entries, SMOD_MAX_DIRECTORY_ENTRIES, &count);
    if (rc != FS_OK) return 0; /* Boot must work without optional modules. */

    for (i = 0u; i < count; ++i) {
        const char* name = entries[i].name;
        char path[FS_PATH_CAP];
        uint32_t j = 0u;
        uint32_t n = 0u;

        if (entries[i].type != FS_NODE_FILE || !smod_has_extension(name) ||
            smod_name_already_loaded(name)) continue;
        if (smod_loaded_count >= SMOD_MAX_LOADED) {
            klog_info("smod", "Module capacity reached");
            break;
        }

        while (j < SMOD_PATH_PREFIX_LENGTH) {
            path[j] = SMOD_PATH_PREFIX[j];
            ++j;
        }
        while (n < FS_NAME_CAP && name[n] != '\0' && j + 1u < FS_PATH_CAP) {
            path[j++] = name[n++];
        }
        if (n == FS_NAME_CAP || name[n] != '\0') {
            klog_info("smod", "Module filename too long");
            continue;
        }
        path[j] = '\0';

        if (smod_core_load_file(path, boot_magic) == 0) {
            ++loaded;
            klog_info("smod", path);
        } else {
            klog_info("smod", "Invalid or unsupported .mod file");
        }
    }
    if (active_rtc) smod_report_clock();
    return loaded;
}
