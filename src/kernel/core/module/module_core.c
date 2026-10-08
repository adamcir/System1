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

static const smod_api_v1_t smod_api = { SMOD_API_VERSION, smod_report_ready };

/* Native v1: code+optional BSS, absolute entry offset and no relocations yet.
 * The .mod image must be self-contained position-independent machine code.
 */
static int smod_core_load_file(const char* path) {
    uint8_t hdr[SMOD_HEADER_SIZE];
    fs_stat_t st;
    uint32_t id = 0u;
    uint32_t image_size;
    uint32_t memory_size;
    uint32_t entry_offset;
    uint32_t i;
    void* image;
    int rc;

    if (smod_loaded_count >= SMOD_MAX_LOADED) return -1;
    if (fs_core_open(path, FS_O_RDONLY, &id) != FS_OK) return -1;

    rc = fs_core_fstat(id, &st);
    if (rc != FS_OK || (st.mode & FS_MODE_FILE) == 0u ||
        st.size < SMOD_HEADER_SIZE || st.size > SMOD_HEADER_SIZE + SMOD_IMAGE_LIMIT ||
        smod_read_exact(id, 0u, hdr, SMOD_HEADER_SIZE) != 0) {
        (void)fs_core_close(id);
        return -1;
    }

    image_size = smod_u32(hdr + 16u);
    memory_size = smod_u32(hdr + 20u);
    entry_offset = smod_u32(hdr + 24u);

    if (hdr[0] != 'S' || hdr[1] != 'M' || hdr[2] != 'O' || hdr[3] != 'D' ||
        smod_u16(hdr + 4u) != SMOD_FORMAT_VERSION ||
        smod_u16(hdr + 6u) != SMOD_API_VERSION ||
        smod_u16(hdr + 8u) != smod_machine_arch() ||
        smod_u16(hdr + 10u) != SMOD_FLAG_EXECUTABLE ||
        smod_u32(hdr + 12u) != SMOD_HEADER_SIZE ||
        smod_u32(hdr + 28u) != 0u ||
        image_size == 0u || image_size > SMOD_IMAGE_LIMIT ||
        memory_size < image_size || memory_size > SMOD_IMAGE_LIMIT ||
        entry_offset >= image_size ||
        st.size != SMOD_HEADER_SIZE + image_size) {
        (void)fs_core_close(id);
        return -1;
    }

    image = kmalloc(memory_size);
    if (image == 0) {
        (void)fs_core_close(id);
        return -1;
    }

    rc = smod_read_exact(id, SMOD_HEADER_SIZE, (uint8_t*)image, image_size);
    (void)fs_core_close(id);
    if (rc != 0) {
        kfree(image);
        return -1;
    }
    for (i = image_size; i < memory_size; ++i) ((uint8_t*)image)[i] = 0u;

    /* Keep the image alive through initialization. Function pointer conversion
     * is valid on supported PC targets with identity-mapped executable heap.
     */
    {
        smod_entry_fn_t entry = (smod_entry_fn_t)((uint8_t*)image + entry_offset);
        if (entry(&smod_api) != 0) {
            kfree(image);
            return -1;
        }
    }

    smod_images[smod_loaded_count++] = image;
    return 0;
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

int smod_core_boot_load_all(void) {
    fs_dirent_t entries[SMOD_MAX_DIRECTORY_ENTRIES];
    uint32_t count = 0u;
    uint32_t i;
    int rc;
    int loaded = 0;

    rc = fs_core_list_dir("/boot/modules", entries, SMOD_MAX_DIRECTORY_ENTRIES, &count);
    if (rc != FS_OK) return 0; /* Boot must work without optional modules. */

    for (i = 0u; i < count; ++i) {
        const char* name = entries[i].name;
        char path[FS_PATH_CAP];
        uint32_t j = 0u;
        uint32_t n = 0u;

        if (entries[i].type != FS_NODE_FILE || !smod_has_extension(name)) continue;
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

        if (smod_core_load_file(path) == 0) {
            ++loaded;
            klog_info("smod", path);
        } else {
            klog_info("smod", "Invalid or unsupported .mod file");
        }
    }
    return loaded;
}
