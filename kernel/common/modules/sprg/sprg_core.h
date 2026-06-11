#ifndef SYSTEM1_COMMON_SPRG_CORE_H
#define SYSTEM1_COMMON_SPRG_CORE_H

#include "types.h"

#define SPRG_MAGIC0 'S'
#define SPRG_MAGIC1 'P'
#define SPRG_MAGIC2 'R'
#define SPRG_MAGIC3 'G'

#define SPRG_VERSION 1u
#define SPRG_ARCH_I386 1u
#define SPRG_ARCH_X86_64 2u
#define SPRG_PHDR_LOAD 1u

#define SPRG_FLAG_R 0x1u
#define SPRG_FLAG_W 0x2u
#define SPRG_FLAG_X 0x4u

#define SPRG_MAX_PHDRS 8u
#define SPRG_MAX_FILE_SIZE 8192u

#define SPRG_OK 0
#define SPRG_ERR_INVALID -1
#define SPRG_ERR_NOT_FOUND -2
#define SPRG_ERR_TOO_LARGE -3
#define SPRG_ERR_BAD_MAGIC -4
#define SPRG_ERR_BAD_VERSION -5
#define SPRG_ERR_BAD_ARCH -6
#define SPRG_ERR_BAD_HEADER -7
#define SPRG_ERR_UNSUPPORTED -8

typedef struct {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
    uint32_t reserved;
} sprg_segment_t;

typedef struct {
    uint32_t entry;
    uint32_t arch;
    uint32_t segment_count;
    uint32_t file_size;
    sprg_segment_t segments[SPRG_MAX_PHDRS];
} sprg_image_t;

int sprg_core_validate_file(const char* path, uint32_t expected_arch, sprg_image_t* out_image);

#endif
