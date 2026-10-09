#include "types.h"
#include "module.h"
#include "module_core.h"
#include "paging.h"
#include "klog.h"

#define SMOD_MB2_MAGIC 0x36D76289u
#define SMOD_FLOPPY_MAGIC 0x53314D47u
#define SMOD_MB2_MODULE_TAG 3u
#define SMOD_MB2_MAX_INFO 65536u
#define SMOD_BOOT_TAG_LIMIT 128u

typedef struct {
    uint32_t type;
    uint32_t size;
} smod_boot_tag_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t begin;
    uint32_t end;
} smod_mb2_module_t;

static int smod_prefix(const char* data, uint32_t max_bytes) {
    return max_bytes >= 6u &&
           data[0] == 's' && data[1] == 'm' && data[2] == 'o' &&
           data[3] == 'd' && data[4] == ':' && data[5] != '\0';
}

int smod_boot_early_init(uint32_t magic, uint32_t info_ptr) {
    uint32_t loaded = 0u;
#if defined(SYSTEM1_FLOPPY_BOOT)
    extern const uint8_t __smod_floppy_com1_begin[];
    extern const uint8_t __smod_floppy_com1_end[];
    extern const uint8_t __smod_floppy_cmos_begin[];
    extern const uint8_t __smod_floppy_cmos_end[];
    extern const uint8_t __smod_floppy_picpit_begin[];
    extern const uint8_t __smod_floppy_picpit_end[];
    extern const uint8_t __smod_floppy_vga_begin[];
    extern const uint8_t __smod_floppy_vga_end[];
    extern const uint8_t __smod_floppy_ps2_begin[];
    extern const uint8_t __smod_floppy_ps2_end[];
    extern const uint8_t __smod_floppy_fdc_begin[];
    extern const uint8_t __smod_floppy_fdc_end[];

    (void)info_ptr;
    if (magic != SMOD_FLOPPY_MAGIC) return 0;
    if (smod_core_preload(__smod_floppy_com1_begin,
        (uint32_t)(__smod_floppy_com1_end - __smod_floppy_com1_begin),
        "com1.mod") == 0) ++loaded;
    if (smod_core_preload(__smod_floppy_cmos_begin,
        (uint32_t)(__smod_floppy_cmos_end - __smod_floppy_cmos_begin),
        "cmos.mod") == 0) ++loaded;
    if (smod_core_preload(__smod_floppy_picpit_begin,
        (uint32_t)(__smod_floppy_picpit_end - __smod_floppy_picpit_begin),
        "picpit.mod") == 0) ++loaded;
    if (smod_core_preload(__smod_floppy_vga_begin,
        (uint32_t)(__smod_floppy_vga_end - __smod_floppy_vga_begin),
        "vga.mod") == 0) ++loaded;
    if (smod_core_preload(__smod_floppy_ps2_begin,
        (uint32_t)(__smod_floppy_ps2_end - __smod_floppy_ps2_begin),
        "ps2.mod") == 0) ++loaded;
    if (smod_core_preload(__smod_floppy_fdc_begin,
        (uint32_t)(__smod_floppy_fdc_end - __smod_floppy_fdc_begin),
        "fdc.mod") == 0) ++loaded;
#else
    const uint8_t* data;
    uint32_t total, off, seen = 0u;
    uintptr_t limit = paging_identity_limit();
    if (magic != SMOD_MB2_MAGIC || info_ptr == 0u ||
        info_ptr >= limit || limit - info_ptr < 16u) return 0;
    data = (const uint8_t*)(uintptr_t)info_ptr;
    total = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
            ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    if (total < 16u || total > SMOD_MB2_MAX_INFO ||
        total > limit - info_ptr) return 0;
    off = 8u;
    while (off <= total - sizeof(smod_boot_tag_t) &&
           seen++ < SMOD_BOOT_TAG_LIMIT) {
        const smod_boot_tag_t* tag = (const smod_boot_tag_t*)(data + off);
        uint32_t step, text_bytes, i;
        const smod_mb2_module_t* mod;
        const char* cmdline;
        uint32_t start, end;
        if (tag->size < sizeof(smod_boot_tag_t) ||
            tag->size > total - off) break;
        if (tag->type == 0u) break;
        if (tag->type == SMOD_MB2_MODULE_TAG &&
            tag->size >= sizeof(smod_mb2_module_t) + 7u) {
            mod = (const smod_mb2_module_t*)tag;
            cmdline = (const char*)tag + sizeof(smod_mb2_module_t);
            text_bytes = tag->size - (uint32_t)sizeof(smod_mb2_module_t);
            if (smod_prefix(cmdline, text_bytes)) {
                /* A missing NUL or long name may never cross tag boundaries. */
                for (i = 5u; i < text_bytes && i < 5u + 24u; ++i)
                    if (cmdline[i] == '\0') break;
                start = mod->begin;
                end = mod->end;
                if (i < text_bytes && i < 5u + 24u &&
                    end > start &&
                    end - start >= SMOD_HEADER_SIZE &&
                    end - start <= SMOD_HEADER_SIZE + SMOD_IMAGE_LIMIT &&
                    start < limit && end <= limit) {
                    if (smod_core_preload((const uint8_t*)(uintptr_t)start,
                            end - start, cmdline + 5u) == 0) ++loaded;
                }
            }
        }
        if (tag->size > 0xFFFFFFF8u) break;
        step = (tag->size + 7u) & ~7u;
        if (step > total - off) break;
        off += step;
    }
#endif
    if (loaded != 0u) klog_info("smod", "Early boot modules activated");
    return (int)loaded;
}
