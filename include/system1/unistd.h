#ifndef SYSTEM1_USER_UNISTD_H
#define SYSTEM1_USER_UNISTD_H

#include "types.h"

#define SYS_READ   0u
#define SYS_WRITE  1u
#define SYS_OPEN   2u
#define SYS_CLOSE  3u
#define SYS_LSEEK  8u
#define SYS_STAT   9u
#define SYS_GETCWD 10u
#define SYS_CHDIR  11u
#define SYS_MKDIR  12u
#define SYS_UNLINK 13u
#define SYS_EXECVE 59u

#define SEEK_SET 0u
#define SEEK_CUR 1u
#define SEEK_END 2u

typedef int (*system1_syscall_handler_t)(uint32_t nr, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

void system1_set_syscall_handler(system1_syscall_handler_t handler);
int system1_syscall(uint32_t nr, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

int open(const char* path, int flags);
int close(int fd);
int read(int fd, void* buf, unsigned count);
int write(int fd, const void* buf, unsigned count);
int lseek(int fd, int offset, unsigned whence);
int chdir(const char* path);
int mkdir(const char* path);
int unlink(const char* path);
int execve(const char* path, char* const argv[], char* const envp[]);
char* getcwd(char* buf, unsigned size);

#endif
