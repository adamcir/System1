#include "types.h"
#include "bootlog.h"
#include "interrupts.h"
#include "keyboard.h"
#include "tty.h"
#include "vga.h"

void kmain_i386(uint32_t magic, uint32_t info) {
    (void)magic;
    (void)info;

    vga_init();
    interrupts_init();
    keyboard_init();
    irq_register_handler(1, keyboard_irq_handler);
    interrupts_enable();
    bootlog_info("System/1 boot via GRUB");
    bootlog_info("modules: interrupts, keyboard, tty");
    vga_set_color(WHITE);
    vga_puts("Starting tty...");
    vga_text_begin(3, 0);
    tty_run();
}
