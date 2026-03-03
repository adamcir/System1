#include "tty-core.h"
#include "types.h"
#include "keyboard.h"
#include "vga.h"

static uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static __attribute__((noreturn)) void tty_core_reboot(void) {
    __asm__ volatile ("cli");
    while (inb(0x64) & 0x02u) {
    }
    outb(0x64, 0xFEu);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void tty_core_run(void) {
    int key;

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

        if (key == KEY_HOME) {
            vga_text_home();
            continue;
        }

        if (key == KEY_END) {
            vga_text_end();
            continue;
        }

        if (key == KEY_INSERT) {
            vga_text_toggle_insert();
            continue;
        }

        if (key == KEY_CTRL_ALT_DEL) {
            tty_core_reboot();
        }

        if (key > 0 && key < 128) {
            vga_text_putc((char)key);
        }
    }
}
