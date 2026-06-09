#ifndef SYSTEM1_COMMON_FD_CORE_H
#define SYSTEM1_COMMON_FD_CORE_H

#include "types.h"

#define POSIX_FD_CAP 32u
#define POSIX_STDIN_FILENO 0
#define POSIX_STDOUT_FILENO 1
#define POSIX_STDERR_FILENO 2

void fd_core_init(void);
int fd_core_open(const char* path, uint32_t flags);
int fd_core_close(int fd);
int fd_core_read(int fd, void* buffer, uint32_t count);
int fd_core_write(int fd, const void* buffer, uint32_t count);
int fd_core_lseek(int fd, int offset, uint32_t whence);

#endif
