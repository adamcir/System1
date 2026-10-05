#include "sprg_core.h"
#include "fs.h"

#define SPRG_HEADER_SIZE 28u
#define SPRG_PHDR_SIZE 32u

static uint8_t g_sprg_file[SPRG_MAX_FILE_SIZE];

static uint32_t sprg_le32(const uint8_t* p) {
    return ((uint32_t)p[0]) |
        ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) |
        ((uint32_t)p[3] << 24);
}

static uint32_t sprg_strlen(const char* s) {
    uint32_t len = 0u;

    if (s == 0) {
        return 0u;
    }

    while (s[len] != '\0') {
        ++len;
    }

    return len;
}

static int sprg_has_prg_suffix(const char* path) {
    uint32_t len = sprg_strlen(path);

    if (len < 4u) {
        return 0;
    }

    return (path[len - 4u] == '.' &&
        path[len - 3u] == 'p' &&
        path[len - 2u] == 'r' &&
        path[len - 1u] == 'g');
}

static int sprg_range_valid(uint32_t offset, uint32_t size, uint32_t file_size) {
    if (offset > file_size) {
        return 0;
    }

    if (size > (file_size - offset)) {
        return 0;
    }

    return 1;
}

static int sprg_segments_overlap(uint32_t left_addr, uint32_t left_size, uint32_t right_addr, uint32_t right_size) {
    uint32_t left_end;
    uint32_t right_end;

    if (left_size == 0u || right_size == 0u) {
        return 0;
    }

    left_end = left_addr + left_size;
    right_end = right_addr + right_size;

    if (left_end < left_addr || right_end < right_addr) {
        return 1;
    }

    return (left_addr < right_end && right_addr < left_end);
}

static int sprg_validate_segment_table(sprg_image_t* image) {
    uint32_t i;
    uint32_t j;

    for (i = 0u; i < image->segment_count; ++i) {
        sprg_segment_t* seg = &image->segments[i];

        if (seg->type != SPRG_PHDR_LOAD) {
            return SPRG_ERR_UNSUPPORTED;
        }

        if (seg->memsz < seg->filesz) {
            return SPRG_ERR_BAD_HEADER;
        }

        if (sprg_range_valid(seg->offset, seg->filesz, image->file_size) == 0) {
            return SPRG_ERR_BAD_HEADER;
        }

        if (seg->reserved != 0u) {
            return SPRG_ERR_BAD_HEADER;
        }

        for (j = i + 1u; j < image->segment_count; ++j) {
            if (sprg_segments_overlap(seg->vaddr, seg->memsz, image->segments[j].vaddr, image->segments[j].memsz) != 0) {
                return SPRG_ERR_BAD_HEADER;
            }
        }
    }

    return SPRG_OK;
}

int sprg_core_validate_file(const char* path, uint32_t expected_arch, sprg_image_t* out_image) {
    fs_stat_t st;
    uint32_t size = 0u;
    uint32_t version;
    uint32_t arch;
    uint32_t phoff;
    uint32_t phnum;
    uint32_t flags;
    uint32_t i;
    int rc;

    if (path == 0 || out_image == 0 || sprg_has_prg_suffix(path) == 0) {
        return SPRG_ERR_INVALID;
    }

    rc = fs_stat(path, &st);
    if (rc != FS_OK) {
        return (rc == FS_ERR_NOT_FOUND) ? SPRG_ERR_NOT_FOUND : SPRG_ERR_INVALID;
    }

    if ((st.mode & FS_MODE_FILE) == 0u) {
        return SPRG_ERR_INVALID;
    }

    if (st.size > SPRG_MAX_FILE_SIZE) {
        return SPRG_ERR_TOO_LARGE;
    }

    rc = fs_read_file(path, (char*)g_sprg_file, SPRG_MAX_FILE_SIZE, &size);
    if (rc != FS_OK) {
        return (rc == FS_ERR_NOT_FOUND) ? SPRG_ERR_NOT_FOUND : SPRG_ERR_INVALID;
    }

    if (size < SPRG_HEADER_SIZE ||
        g_sprg_file[0] != SPRG_MAGIC0 ||
        g_sprg_file[1] != SPRG_MAGIC1 ||
        g_sprg_file[2] != SPRG_MAGIC2 ||
        g_sprg_file[3] != SPRG_MAGIC3) {
        return SPRG_ERR_BAD_MAGIC;
    }

    version = sprg_le32(&g_sprg_file[4]);
    arch = sprg_le32(&g_sprg_file[8]);
    out_image->entry = sprg_le32(&g_sprg_file[12]);
    phoff = sprg_le32(&g_sprg_file[16]);
    phnum = sprg_le32(&g_sprg_file[20]);
    flags = sprg_le32(&g_sprg_file[24]);

    if (version != SPRG_VERSION) {
        return SPRG_ERR_BAD_VERSION;
    }

    if (arch != expected_arch) {
        return SPRG_ERR_BAD_ARCH;
    }

    if (flags != 0u || phnum == 0u || phnum > SPRG_MAX_PHDRS) {
        return SPRG_ERR_BAD_HEADER;
    }

    if (sprg_range_valid(phoff, phnum * SPRG_PHDR_SIZE, size) == 0) {
        return SPRG_ERR_BAD_HEADER;
    }

    out_image->arch = arch;
    out_image->segment_count = phnum;
    out_image->file_size = size;

    for (i = 0u; i < phnum; ++i) {
        const uint8_t* p = &g_sprg_file[phoff + (i * SPRG_PHDR_SIZE)];
        out_image->segments[i].type = sprg_le32(&p[0]);
        out_image->segments[i].offset = sprg_le32(&p[4]);
        out_image->segments[i].vaddr = sprg_le32(&p[8]);
        out_image->segments[i].filesz = sprg_le32(&p[12]);
        out_image->segments[i].memsz = sprg_le32(&p[16]);
        out_image->segments[i].flags = sprg_le32(&p[20]);
        out_image->segments[i].align = sprg_le32(&p[24]);
        out_image->segments[i].reserved = sprg_le32(&p[28]);
    }

    return sprg_validate_segment_table(out_image);
}
