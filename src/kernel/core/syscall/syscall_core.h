#ifndef SYSTEM1_COMMON_SYSCALL_CORE_H
#define SYSTEM1_COMMON_SYSCALL_CORE_H

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

void syscall_core_init(void);
int syscall_core_dispatch(uint32_t nr, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

#endif
