#ifndef SYSTEM1_I386_FLOPPY_TTY_H
#define SYSTEM1_I386_FLOPPY_TTY_H

#include "types.h"

#ifndef SYSTEM1_TTY_SHARED_DECLS
#define SYSTEM1_TTY_SHARED_DECLS

typedef void (*tty_tab_hook_t)(char* buf, uint32_t cap, uint32_t* len, uint32_t* cursor, void* ctx);
typedef int (*tty_history_hook_t)(int direction, char* buf, uint32_t cap, uint32_t* out_len, uint32_t* out_cursor, void* ctx);

typedef struct {
    tty_tab_hook_t tab_hook;
    tty_history_hook_t history_hook;
    void* ctx;
} tty_readline_hooks_t;

#endif

int tty_readline(char* buf, uint32_t cap);
int tty_readline_ex(char* buf, uint32_t cap, const tty_readline_hooks_t* hooks);

#endif
