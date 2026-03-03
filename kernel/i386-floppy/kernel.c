#include "types.h"
#include "bootlog.h"
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
    if (magic != FLOPPY_MAGIC) {
        bootlog_info("Bad boot magic");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    bootlog_info("System/1 boot via floppy loader");
    vga_puts("drive: ");
    vga_hex_u32(info->boot_drive);
    vga_puts("\nkernel: ");
    vga_hex_u32(info->kernel_load_addr);
    vga_puts("\nsize: ");
    vga_hex_u32(info->kernel_size_bytes);
    vga_puts("\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
