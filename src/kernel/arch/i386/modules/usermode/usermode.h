#ifndef SYSTEM1_I386_USERMODE_H
#define SYSTEM1_I386_USERMODE_H

int usermode_init(void);
int usermode_exec(const char* path, char* const argv[], char* const envp[]);
void usermode_exit(int status) __attribute__((noreturn));

#endif
