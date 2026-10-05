#ifndef SYSTEM1_I386_FLOPPY_USERMODE_H
#define SYSTEM1_I386_FLOPPY_USERMODE_H

int usermode_init(void);
int usermode_exec(const char* path);
void usermode_exit(int status) __attribute__((noreturn));

#endif
