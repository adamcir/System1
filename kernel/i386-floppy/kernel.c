#include "types.h"
#include "klog.h"
#include "interrupts.h"
#include "keyboard.h"
#include "shell.h"
#include "vga.h"
#include "paging.h"
#include "fs.h"

#define FLOPPY_MAGIC 0x53314D47u

typedef struct {
    uint32_t boot_drive;
    uint32_t kernel_load_addr;
    uint32_t kernel_size_bytes;
    uint32_t boot_sector_addr;
    uint32_t fat_addr;
    uint32_t root_dir_addr;
    uint32_t fat_lba;
    uint32_t fat_sectors;
    uint32_t root_lba;
    uint32_t root_sectors;
    uint32_t floppy_image_addr;
} boot_info_t;

void kmain_floppy_i386(uint32_t magic, uint32_t boot_info_ptr) {
    boot_info_t* info = (boot_info_t*)(uintptr_t)boot_info_ptr;
    

    vga_init();
    if (paging_init(magic, boot_info_ptr) != 0) {
        panic("paging_init failed");
    }
    interrupts_init();
    keyboard_init();
    irq_register_handler(1, keyboard_irq_handler);
    interrupts_enable();
    if (magic != FLOPPY_MAGIC) {
        klog_info("boot - fatal", "Bad boot magic");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    klog_info("boot", "System/1 boot via floppy loader");
    klog_info("kernel", "modules: vga, signals, interrupts, keyboard, shell");
    fs_set_boot_context(magic, boot_info_ptr);
    if (fs_init() != FS_OK) {
        panic("Unable to mount FS");
    }
    vga_set_color(GREEN);
    vga_puts("drive: ");
    vga_hex_u32(info->boot_drive);
    vga_puts("\nkernel: ");
    vga_hex_u32(info->kernel_load_addr);
    vga_puts("\nsize: ");
    vga_hex_u32(info->kernel_size_bytes);
    vga_puts("\n");
    klog_system_logo();
    shell_run();
    panic("Kernel loop ended!");
}
