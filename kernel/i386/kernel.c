#include "types.h"
#include "bootlog.h"
#include "interrupts.h"
#include "keyboard.h"
#include "vga.h"

void kmain_i386(uint32_t magic, uint32_t info) {
    int key;
    (void)magic;
    (void)info;

    vga_init();
    interrupts_init();
    keyboard_init();
    irq_register_handler(1, keyboard_irq_handler);
    interrupts_enable();
    bootlog_info("System/1 boot via GRUB");
    bootlog_info("modules: interrupts, keyboard");
    vga_text_begin(2, 0);

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

        if (key > 0 && key < 128) {
            vga_text_putc((char)key);
        }
    }
}
