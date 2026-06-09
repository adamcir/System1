#ifndef SYSTEM1_COMMON_TTY_CORE_H
#define SYSTEM1_COMMON_TTY_CORE_H

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

typedef enum {
    TTY_BLACK = 0,
    TTY_BLUE = 1,
    TTY_GREEN = 2,
    TTY_CYAN = 3,
    TTY_RED = 4,
    TTY_MAGENTA = 5,
    TTY_BROWN = 6,
    TTY_LIGHT_GREY = 7,
    TTY_DARK_GREY = 8,
    TTY_LIGHT_BLUE = 9,
    TTY_LIGHT_GREEN = 10,
    TTY_LIGHT_CYAN = 11,
    TTY_LIGHT_RED = 12,
    TTY_LIGHT_MAGENTA = 13,
    TTY_LIGHT_BROWN = 14,
    TTY_YELLOW = 14,
    TTY_WHITE = 15
} tty_color_t;

#endif

void tty_core_init(void);
void tty_core_clear(void);
void tty_core_set_color(tty_color_t color);
void tty_core_putc(char c);
void tty_core_puts(const char* s);
void tty_core_hex_u32(uint32_t value);
void tty_core_get_cursor(uint16_t* out_row, uint16_t* out_col);
void tty_core_text_begin(uint16_t row, uint16_t col);
int tty_core_readline(char* buf, uint32_t cap);
int tty_core_readline_ex(char* buf, uint32_t cap, const tty_readline_hooks_t* hooks);

#endif
