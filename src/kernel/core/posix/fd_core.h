#ifndef SYSTEM1_COMMON_FD_CORE_H
#define SYSTEM1_COMMON_FD_CORE_H

#include "fs_core.h"
#include "types.h"

#define POSIX_FD_CAP 32u
#define POSIX_STDIN_FILENO 0
#define POSIX_STDOUT_FILENO 1
#define POSIX_STDERR_FILENO 2

typedef enum {
    FD_KIND_UNUSED = 0,
    FD_KIND_TTY_IN = 1,
    FD_KIND_TTY_OUT = 2,
    FD_KIND_VFS = 3
} fd_kind_t;

typedef struct {
    uint8_t used;
    fd_kind_t kind;
    uint32_t node_id;
    uint32_t offset;
    uint32_t flags;
} fd_entry_t;

typedef struct {
    fd_entry_t entries[POSIX_FD_CAP];
    uint8_t initialized;
} fd_table_t;

void fd_core_init(void);
void fd_core_table_init(fd_table_t* table);
int fd_core_open(const char* path, uint32_t flags);
int fd_core_close(int fd);
int fd_core_read(int fd, void* buffer, uint32_t count);
int fd_core_write(int fd, const void* buffer, uint32_t count);
int fd_core_lseek(int fd, int offset, uint32_t whence);
int fd_core_fstat(int fd, fs_stat_t* out_stat);

#endif
