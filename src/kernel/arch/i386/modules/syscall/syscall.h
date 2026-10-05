#ifndef SYSTEM1_I386_SYSCALL_H
#define SYSTEM1_I386_SYSCALL_H

#include "syscall_core.h"

void syscall_init(void);
int syscall_dispatch(uint32_t nr, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

#endif
