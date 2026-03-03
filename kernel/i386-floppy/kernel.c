#include "types.h"
#include "bootlog.h"
#include "interrupts.h"
#include "keyboard.h"
#include "tty.h"
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
        bootlog_info("Bad boot magic");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    bootlog_info("System/1 boot via floppy loader");
    bootlog_info("modules: interrupts, keyboard, tty");
    vga_set_color(GREEN);
    vga_puts("drive: ");
    vga_hex_u32(info->boot_drive);
    vga_puts("\nkernel: ");
    vga_hex_u32(info->kernel_load_addr);
    vga_puts("\nsize: ");
    vga_hex_u32(info->kernel_size_bytes);
    vga_puts("\n");
    vga_set_color(WHITE);
    vga_puts("Starting tty...");
    vga_text_begin(6, 0);
    tty_run();
}
