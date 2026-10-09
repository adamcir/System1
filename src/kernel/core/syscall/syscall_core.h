#ifndef SYSTEM1_COMMON_SYSCALL_CORE_H
#define SYSTEM1_COMMON_SYSCALL_CORE_H

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

void syscall_core_init(void);
int syscall_core_dispatch(uint32_t nr, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

#endif
