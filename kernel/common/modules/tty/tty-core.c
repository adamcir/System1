#include "tty-core.h"
#include "types.h"
#include "keyboard.h"
#include "signals.h"
#include "vga.h"

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
