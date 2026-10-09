#ifndef SYSTEM1_USER_UNISTD_H
#define SYSTEM1_USER_UNISTD_H

#include "types.h"

#define SYS_READ   0u
#define SYS_WRITE  1u
#define SYS_OPEN   2u
#define SYS_CLOSE  3u
#define SYS_FSTAT  5u
#define SYS_LSEEK  8u
#define SYS_STAT   9u
#define SYS_GETCWD 10u
#define SYS_CHDIR  11u
#define SYS_MKDIR  12u
#define SYS_UNLINK 13u
#define SYS_RMDIR 14u
#define SYS_IOCTL 16u
#define SYS_ACCESS 21u
#define SYS_DUP 32u
#define SYS_DUP2 33u
#define SYS_NANOSLEEP 35u
#define SYS_SYNC 36u
#define SYS_GETPID 39u
#define SYS_REBOOT 88u
#define SYS_ISATTY 89u
#define SYS_GETPPID 110u
#define SYS_SYMLINK 83u
#define SYS_READLINK 85u
#define SYS_GETDENTS 141u
#define SYS_EXECVE 59u
#define SYS_EXIT   60u

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

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
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int isatty(int fd);
int access(const char* path, int mode);
int getpid(void);
int getppid(void);
unsigned sleep(unsigned seconds);
int usleep(unsigned usec);
void sync(void);
int chdir(const char* path);
int mkdir(const char* path);
int unlink(const char* path);
int rmdir(const char* path);
int symlink(const char* target, const char* linkpath);
int readlink(const char* path, char* buf, unsigned size);
int execve(const char* path, char* const argv[], char* const envp[]);
void _exit(int status) __attribute__((noreturn));
char* getcwd(char* buf, unsigned size);

#endif
