#include "sprg_core.h"
#include "fs.h"
#include "fs_core.h"

#define SPRG_HEADER_SIZE 28u
#define SPRG_PHDR_SIZE 32u
#define SPRG_STREAM_CHUNK 512u

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

static int sprg_segments_overlap(uint32_t left_addr, uint32_t left_size,
                                 uint32_t right_addr, uint32_t right_size) {
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
            if (sprg_segments_overlap(seg->vaddr, seg->memsz,
                                      image->segments[j].vaddr,
                                      image->segments[j].memsz) != 0) {
                return SPRG_ERR_BAD_HEADER;
            }
        }
    }

    return SPRG_OK;
}

static int sprg_read_exact(uint32_t node_id, uint32_t offset,
                           uint8_t* buffer, uint32_t size) {
    uint32_t done = 0u;

    while (done < size) {
        uint32_t chunk = size - done;
        uint32_t got = 0u;
        int rc;

        if (chunk > SPRG_STREAM_CHUNK) {
            chunk = SPRG_STREAM_CHUNK;
        }

        rc = fs_core_read(node_id, offset + done,
                          (char*)(buffer + done), chunk, &got);
        if (rc != FS_OK || got == 0u) {
            return SPRG_ERR_INVALID;
        }

        done += got;
    }

    return SPRG_OK;
}

int sprg_core_validate_file(const char* path, uint32_t expected_arch,
                            sprg_image_t* out_image) {
    uint8_t header[SPRG_HEADER_SIZE];
    uint8_t phdr[SPRG_PHDR_SIZE];
    fs_stat_t st;
    uint32_t node_id = 0u;
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

    rc = fs_core_open(path, FS_O_RDONLY, &node_id);
    if (rc != FS_OK) {
        return (rc == FS_ERR_NOT_FOUND) ? SPRG_ERR_NOT_FOUND : SPRG_ERR_INVALID;
    }

    rc = sprg_read_exact(node_id, 0u, header, SPRG_HEADER_SIZE);
    if (rc != SPRG_OK) {
        (void)fs_core_close(node_id);
        return rc;
    }

    if (header[0] != SPRG_MAGIC0 ||
        header[1] != SPRG_MAGIC1 ||
        header[2] != SPRG_MAGIC2 ||
        header[3] != SPRG_MAGIC3) {
        (void)fs_core_close(node_id);
        return SPRG_ERR_BAD_MAGIC;
    }

    version = sprg_le32(&header[4]);
    arch = sprg_le32(&header[8]);
    out_image->entry = sprg_le32(&header[12]);
    phoff = sprg_le32(&header[16]);
    phnum = sprg_le32(&header[20]);
    flags = sprg_le32(&header[24]);

    if (version != SPRG_VERSION) {
        (void)fs_core_close(node_id);
        return SPRG_ERR_BAD_VERSION;
    }

    if (arch != expected_arch) {
        (void)fs_core_close(node_id);
        return SPRG_ERR_BAD_ARCH;
    }

    if (flags != 0u || phnum == 0u || phnum > SPRG_MAX_PHDRS) {
        (void)fs_core_close(node_id);
        return SPRG_ERR_BAD_HEADER;
    }

    if (sprg_range_valid(phoff, phnum * SPRG_PHDR_SIZE, st.size) == 0) {
        (void)fs_core_close(node_id);
        return SPRG_ERR_BAD_HEADER;
    }

    out_image->arch = arch;
    out_image->segment_count = phnum;
    out_image->file_size = st.size;

    for (i = 0u; i < phnum; ++i) {
        sprg_segment_t* seg = &out_image->segments[i];

        rc = sprg_read_exact(node_id, phoff + (i * SPRG_PHDR_SIZE),
                             phdr, SPRG_PHDR_SIZE);
        if (rc != SPRG_OK) {
            (void)fs_core_close(node_id);
            return rc;
        }

        seg->type = sprg_le32(&phdr[0]);
        seg->offset = sprg_le32(&phdr[4]);
        seg->vaddr = sprg_le32(&phdr[8]);
        seg->filesz = sprg_le32(&phdr[12]);
        seg->memsz = sprg_le32(&phdr[16]);
        seg->flags = sprg_le32(&phdr[20]);
        seg->align = sprg_le32(&phdr[24]);
        seg->reserved = sprg_le32(&phdr[28]);
    }

    (void)fs_core_close(node_id);
    return sprg_validate_segment_table(out_image);
}

int sprg_core_load_file(const char* path, uint32_t expected_arch,
                        uintptr_t user_min, uintptr_t user_max,
                        sprg_image_t* out_image) {
    uint32_t node_id = 0u;
    uint32_t i;
    uint8_t entry_is_executable = 0u;
    int rc;

    if (user_min >= user_max) {
        return SPRG_ERR_INVALID;
    }

    rc = sprg_core_validate_file(path, expected_arch, out_image);
    if (rc != SPRG_OK) {
        return rc;
    }

    rc = fs_core_open(path, FS_O_RDONLY, &node_id);
    if (rc != FS_OK) {
        return (rc == FS_ERR_NOT_FOUND) ? SPRG_ERR_NOT_FOUND : SPRG_ERR_INVALID;
    }

    for (i = 0u; i < out_image->segment_count; ++i) {
        sprg_segment_t* seg = &out_image->segments[i];
        uintptr_t start = (uintptr_t)seg->vaddr;
        uintptr_t end = start + (uintptr_t)seg->memsz;
        uint32_t done = 0u;
        uint32_t j;

        if (end < start || start < user_min || end > user_max) {
            (void)fs_core_close(node_id);
            return SPRG_ERR_BAD_HEADER;
        }

        if ((seg->flags & ~(SPRG_FLAG_R | SPRG_FLAG_W | SPRG_FLAG_X)) != 0u) {
            (void)fs_core_close(node_id);
            return SPRG_ERR_BAD_HEADER;
        }

        if ((seg->flags & SPRG_FLAG_X) != 0u &&
            (uintptr_t)out_image->entry >= start &&
            (uintptr_t)out_image->entry < end) {
            entry_is_executable = 1u;
        }

        while (done < seg->filesz) {
            uint32_t chunk = seg->filesz - done;
            uint32_t got = 0u;

            if (chunk > SPRG_STREAM_CHUNK) {
                chunk = SPRG_STREAM_CHUNK;
            }

            rc = fs_core_read(node_id, seg->offset + done,
                              (char*)(start + done), chunk, &got);
            if (rc != FS_OK || got == 0u) {
                (void)fs_core_close(node_id);
                return SPRG_ERR_INVALID;
            }

            done += got;
        }

        for (j = seg->filesz; j < seg->memsz; ++j) {
            ((uint8_t*)start)[j] = 0u;
        }
    }

    (void)fs_core_close(node_id);

    if (entry_is_executable == 0u) {
        return SPRG_ERR_BAD_HEADER;
    }

    return SPRG_OK;
}
