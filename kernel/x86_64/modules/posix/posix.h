#ifndef SYSTEM1_X86_64_POSIX_H
#define SYSTEM1_X86_64_POSIX_H

#include "types.h"
#include "fs.h"
#include "posix_errno.h"

#define POSIX_STDIN_FILENO 0
#define POSIX_STDOUT_FILENO 1
#define POSIX_STDERR_FILENO 2

void posix_init(void);
int posix_open(const char* path, uint32_t flags);
int posix_close(int fd);
int posix_read(int fd, void* buffer, uint32_t count);
int posix_write(int fd, const void* buffer, uint32_t count);
int posix_lseek(int fd, int offset, uint32_t whence);
int posix_stat(const char* path, fs_stat_t* out_stat);
int posix_fstat(int fd, fs_stat_t* out_stat);
int posix_unlink(const char* path);

#endif
