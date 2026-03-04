#ifndef SYSTEM1_COMMON_TTY_CORE_H
#define SYSTEM1_COMMON_TTY_CORE_H

#include "types.h"

void tty_core_run(void);
int tty_core_readline(char* buf, uint32_t cap);

#endif
