#include "types.h"
#include "klog.h"
#include "interrupts.h"
#include "keyboard.h"
#include "shell.h"
#include "tty.h"
#include "paging.h"
#include "mm.h"
#include "fs.h"
#include "syscall.h"

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

    tty_init();
    if (paging_init(magic, boot_info_ptr) != 0) {
        panic("Paging_init failed");
    }
    klog_info("paging", "Initialized");

    if (mm_init(magic, boot_info_ptr) != 0) {
        panic("MM_init failed");
    }
    klog_info("mm", "Initialized");

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
    klog_info("kernel", "Modules: vga, signals, interrupts, keyboard, shell");
    fs_set_boot_context(magic, boot_info_ptr);
    if (fs_init() != FS_OK) {
        panic("Unable to mount FS");
    }
    syscall_init();
    tty_set_color(TTY_GREEN);
    tty_puts("drive: ");
    tty_hex_u32(info->boot_drive);
    tty_puts("\nkernel: ");
    tty_hex_u32(info->kernel_load_addr);
    tty_puts("\nsize: ");
    tty_hex_u32(info->kernel_size_bytes);
    tty_puts("\n");
    klog_info("mm", "Initial stats");
    mm_print_stats();
    klog_system_logo();
    shell_run();
    panic("Kernel loop ended!");
}
