#ifndef SYSTEM1_USER_STAT_H
#define SYSTEM1_USER_STAT_H

#include "types.h"

#define S_IFDIR  0040000u
#define S_IFREG  0100000u

struct stat {
    uint32_t st_mode;
    uint32_t st_size;
};

int stat(const char* path, struct stat* out_stat);

#endif
