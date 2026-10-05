#ifndef SYSTEM1_COMMON_SYSTEM1_DIRENT_H
#define SYSTEM1_COMMON_SYSTEM1_DIRENT_H

#include "types.h"

#define SYSTEM1_DIRENT_NAME_CAP 24u

typedef struct {
    char d_name[SYSTEM1_DIRENT_NAME_CAP];
    uint8_t d_type;
} system1_dirent_t;

#endif
