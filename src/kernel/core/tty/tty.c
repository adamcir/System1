#include "tty.h"
#include "tty_core.h"

void tty_init(void) {
    tty_core_init();
}

void tty_clear(void) {
    tty_core_clear();
}

void tty_set_color(tty_color_t color) {
    tty_core_set_color(color);
}

void tty_putc(char c) {
    tty_core_putc(c);
}

void tty_puts(const char* s) {
    tty_core_puts(s);
}

void tty_hex_u32(uint32_t value) {
    tty_core_hex_u32(value);
}

void tty_get_cursor(uint16_t* out_row, uint16_t* out_col) {
    tty_core_get_cursor(out_row, out_col);
}

void tty_text_begin(uint16_t row, uint16_t col) {
    tty_core_text_begin(row, col);
}

int tty_readline(char* buf, uint32_t cap) {
    return tty_core_readline(buf, cap);
}

int tty_readline_ex(char* buf, uint32_t cap, const tty_readline_hooks_t* hooks) {
    return tty_core_readline_ex(buf, cap, hooks);
}
