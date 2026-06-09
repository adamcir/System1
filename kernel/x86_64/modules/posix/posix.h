#ifndef SYSTEM1_X86_64_POSIX_H
#define SYSTEM1_X86_64_POSIX_H

#include "types.h"
#include "posix_errno.h"

void posix_init(void);
int posix_open(const char* path, uint32_t flags);
int posix_close(int fd);
int posix_read(int fd, void* buffer, uint32_t count);
int posix_write(int fd, const void* buffer, uint32_t count);
int posix_lseek(int fd, int offset, uint32_t whence);

#endif
