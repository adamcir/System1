#ifndef SYSTEM1_I386_TTY_H
#define SYSTEM1_I386_TTY_H

#include "types.h"

void tty_run(void);
int tty_readline(char* buf, uint32_t cap);

#endif
