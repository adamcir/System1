#include "types.h"
#include "bootlog.h"
#include "interrupts.h"
#include "keyboard.h"
#include "shell.h"
#include "vga.h"

void kmain_x86_64(uint32_t magic, uint32_t info) {
    (void)magic;
    (void)info;

    vga_init();
    interrupts_init();
    keyboard_init();
    irq_register_handler(1, keyboard_irq_handler);
    interrupts_enable();
    bootlog_info("boot", "System/1 boot via GRUB");
    bootlog_info("kernel", "modules: panic, vga, signals, interrupts, keyboard, shell");
    vga_set_color(WHITE);
    vga_puts("==================================================\n");
    vga_puts("   ____            _                     __  __\n");
    vga_puts("  / ___| _   _ ___| |_ ___ _ __ ___     / / /_ |\n");
    vga_puts("  \\___ \\| | | / __| __/ _ \\ '_ ` _ \\   / /   | |\n");
    vga_puts("   ___) | |_| \\__ \\ ||  __/ | | | | | / /    | |\n");
    vga_puts("  |____/ \\__, |___/\\__\\___|_| |_| |_|/ /     |_|\n");
    vga_puts("         |___/                      /_/\n\n");
    vga_puts("==================================================\n");
    vga_puts("=[System/1 by AdavaSoftware (C) 2026]=============\n\n");
    shell_run();
}
