#include "types.h"
#include "bootlog.h"
#include "interrupts.h"
#include "keyboard.h"
#include "vga.h"

#define FLOPPY_MAGIC 0x53314D47u

typedef struct {
    uint32_t boot_drive;
    uint32_t kernel_load_addr;
    uint32_t kernel_size_bytes;
} boot_info_t;

void kmain_floppy_i386(uint32_t magic, uint32_t boot_info_ptr) {
    int key;
    unsigned short row;
    unsigned short col;
    boot_info_t* info = (boot_info_t*)(uintptr_t)boot_info_ptr;

    vga_init();
    interrupts_init();
    keyboard_init();
    if (magic != FLOPPY_MAGIC) {
        bootlog_info("Bad boot magic");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    bootlog_info("System/1 boot via floppy loader");
    bootlog_info("modules: interrupts, keyboard");
    vga_puts("drive: ");
    vga_hex_u32(info->boot_drive);
    vga_puts("\nkernel: ");
    vga_hex_u32(info->kernel_load_addr);
    vga_puts("\nsize: ");
    vga_hex_u32(info->kernel_size_bytes);
    vga_puts("\n");
    vga_set_cursor(5, 0);

    for (;;) {
        keyboard_poll();
        key = keyboard_take_key();
        if (key == KEY_NONE) {
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
