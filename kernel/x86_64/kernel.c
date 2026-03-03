#include "types.h"
#include "bootlog.h"
#include "interrupts.h"
#include "keyboard.h"
#include "vga.h"

void kmain_x86_64(uint32_t magic, uint32_t info) {
    int key;
    unsigned short row;
    unsigned short col;
    (void)magic;
    (void)info;

    vga_init();
    interrupts_init();
    keyboard_init();
    irq_register_handler(1, keyboard_irq_handler);
    interrupts_enable();
    bootlog_info("System/1 boot via GRUB");
    bootlog_info("modules: interrupts, keyboard");
    vga_set_cursor(2, 0);

    for (;;) {
        keyboard_poll();
        key = keyboard_take_key();
        if (key == KEY_NONE) {
            __asm__ volatile ("hlt");
            continue;
        }

        if (key == '\b') {
            vga_get_cursor(&row, &col);
            if (col > 0) {
                col--;
            } else if (row > 0) {
                row--;
                col = 79;
            }
            vga_putc_at(row, col, ' ');
            vga_set_cursor(row, col);
            continue;
        }

        if (key == KEY_LEFT) {
            vga_get_cursor(&row, &col);
            if (col > 0) {
                col--;
            } else if (row > 0) {
                row--;
                col = 79;
            }
            vga_set_cursor(row, col);
            continue;
        }

        if (key == KEY_RIGHT) {
            vga_get_cursor(&row, &col);
            if (col < 79) {
                col++;
            } else if (row < 24) {
                row++;
                col = 0;
            }
            vga_set_cursor(row, col);
            continue;
        }

        if (key > 0 && key < 128) {
            vga_putc((char)key);
        }
    }
}
