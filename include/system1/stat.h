#ifndef SYSTEM1_USER_STAT_H
#define SYSTEM1_USER_STAT_H

#include "types.h"

#define S_IFMT   0170000u
#define S_IFCHR  0020000u
#define S_IFDIR  0040000u
#define S_IFREG  0100000u
#define S_IFLNK  0120000u

#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)

struct stat {
    uint32_t st_mode;
    uint32_t st_size;
};

int stat(const char* path, struct stat* out_stat);
int fstat(int fd, struct stat* out_stat);

#endif
