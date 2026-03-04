#include "types.h"
#include "bootlog.h"
#include "interrupts.h"
#include "keyboard.h"
#include "shell.h"
#include "vga.h"

#define FLOPPY_MAGIC 0x53314D47u

typedef struct {
    uint32_t boot_drive;
    uint32_t kernel_load_addr;
    uint32_t kernel_size_bytes;
} boot_info_t;

void kmain_floppy_i386(uint32_t magic, uint32_t boot_info_ptr) {
    boot_info_t* info = (boot_info_t*)(uintptr_t)boot_info_ptr;
    

    vga_init();
    interrupts_init();
    keyboard_init();
    irq_register_handler(1, keyboard_irq_handler);
    interrupts_enable();
    if (magic != FLOPPY_MAGIC) {
        bootlog_info("boot - fatal", "Bad boot magic");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    bootlog_info("boot", "System/1 boot via floppy loader");
    bootlog_info("kernel", "modules: panic, vga, signals, interrupts, keyboard, shell");
    vga_set_color(GREEN);
    vga_puts("drive: ");
    vga_hex_u32(info->boot_drive);
    vga_puts("\nkernel: ");
    vga_hex_u32(info->kernel_load_addr);
    vga_puts("\nsize: ");
    vga_hex_u32(info->kernel_size_bytes);
    vga_puts("\n");
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
