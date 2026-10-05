#ifndef SYSTEM1_USER_DIRENT_H
#define SYSTEM1_USER_DIRENT_H

#include "types.h"

#define SYSTEM1_DIRENT_NAME_CAP 24u

#define DT_DIR  1u
#define DT_REG  2u
#define DT_LNK  3u

struct dirent {
    char d_name[SYSTEM1_DIRENT_NAME_CAP];
    uint8_t d_type;
};

int getdents(const char* path, struct dirent* entries, unsigned cap);

#endif
