#include "tty-core.h"
#include "types.h"
#include "keyboard.h"
#include "signals.h"
#include "vga.h"

static int tty_insert_char(char* buf, uint32_t cap, uint32_t* len, uint32_t* cursor, uint8_t insert_mode, char c) {
    uint32_t i;

    if (cap == 0u || *len >= (cap - 1u)) {
        return 0;
    }

    if (insert_mode == 0u && *cursor < *len) {
        buf[*cursor] = c;
        (*cursor)++;
        return 1;
    }

    for (i = *len; i > *cursor; --i) {
        buf[i] = buf[i - 1u];
    }

    buf[*cursor] = c;
    (*len)++;
    (*cursor)++;
    return 1;
}

static void tty_delete_left(char* buf, uint32_t* len, uint32_t* cursor) {
    uint32_t i;

    if (*cursor == 0u) {
        return;
    }

    for (i = *cursor; i < *len; ++i) {
        buf[i - 1u] = buf[i];
    }

    (*cursor)--;
    (*len)--;
}

static void tty_delete_at_cursor(char* buf, uint32_t* len, uint32_t cursor) {
    uint32_t i;

    if (cursor >= *len) {
        return;
    }

    for (i = (uint32_t)(cursor + 1u); i < *len; ++i) {
        buf[i - 1u] = buf[i];
    }

    (*len)--;
}

int tty_core_readline(char* buf, uint32_t cap) {
    int key;
    uint32_t len = 0u;
    uint32_t cursor = 0u;
    uint8_t insert_mode = 1u;
    uint16_t row = 0u;
    uint16_t col = 0u;
    int i;

    if (buf == 0 || cap == 0u) {
        return 0;
    }

    buf[0] = '\0';
    vga_get_cursor(&row, &col);
    vga_text_begin(row, col);

    for (;;) {
        keyboard_poll();
        key = keyboard_take_key();
        if (key == KEY_NONE) {
            __asm__ volatile ("hlt");
            continue;
        }

        if (key == KEY_CTRL_ALT_DEL) {
            signal_raise(HW_RESET);
            continue;
        }

        if (key == '\n') {
            buf[len] = '\0';
            vga_putc('\n');
            return (int)len;
        }

        if (key == '\b') {
            if (cursor > 0u) {
                tty_delete_left(buf, &len, &cursor);
                vga_text_backspace();
            }
            continue;
        }

        if (key == KEY_LEFT) {
            if (cursor > 0u) {
                cursor--;
                vga_text_left();
            }
            continue;
        }

        if (key == KEY_RIGHT) {
            if (cursor < len) {
                cursor++;
                vga_text_right();
            }
            continue;
        }

        if (key == KEY_DELETE) {
            if (cursor < len) {
                tty_delete_at_cursor(buf, &len, cursor);
                vga_text_delete();
            }
            continue;
        }

        if (key == KEY_INSERT) {
            insert_mode = (uint8_t)(insert_mode ? 0u : 1u);
            vga_text_toggle_insert();
            continue;
        }

        if (key == '\t') {
            for (i = 0; i < 4; ++i) {
                if (tty_insert_char(buf, cap, &len, &cursor, insert_mode, ' ') == 0) {
                    break;
                }
                vga_text_putc(' ');
            }
            continue;
        }

        if (key > 0 && key < 128) {
            if (tty_insert_char(buf, cap, &len, &cursor, insert_mode, (char)key) != 0) {
                vga_text_putc((char)key);
            }
            continue;
        }
    }
}

void tty_core_run(void) {
    int key;
    int i;

    for (;;) {
        keyboard_poll();
        key = keyboard_take_key();
        if (key == KEY_NONE) {
            __asm__ volatile ("hlt");
            continue;
        }

        if (key == '\b') {
            vga_text_backspace();
            continue;
        }

        if (key == KEY_LEFT) {
            vga_text_left();
            continue;
        }

        if (key == KEY_RIGHT) {
            vga_text_right();
            continue;
        }

        if (key == KEY_DELETE) {
            vga_text_delete();
            continue;
        }

        if (key == KEY_INSERT) {
            vga_text_toggle_insert();
            continue;
        }

        if (key == '\t') {
            for (i = 0; i < 4; ++i) {
                vga_text_putc(' ');
            }
            continue;
        }

        if (key == KEY_CTRL_ALT_DEL) {
            signal_raise(HW_RESET);
            continue;
        }

        if (key > 0 && key < 128) {
            vga_text_putc((char)key);
        }
    }
}
