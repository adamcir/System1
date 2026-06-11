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

void kmain_i386(uint32_t magic, uint32_t info) {
    tty_init();

    if (paging_init(magic, info) != 0) {
        panic("Paging_init failed");
    }
    klog_info("paging", "Initialized");

    if (mm_init(magic, info) != 0) {
        panic("MM_init failed");
    }
    klog_info("mm", "Initialized");

    interrupts_init();
    keyboard_init();
    irq_register_handler(1, keyboard_irq_handler);
    interrupts_enable();
    klog_info("boot", "System/1 boot via GRUB");
    klog_info("kernel", "Modules: vga, signals, interrupts, keyboard, shell");
    fs_set_boot_context(magic, info);
    if (fs_init() != FS_OK) {
        panic("Unable to mount FS");
    }
    syscall_init();
    klog_info("mm", "Initial stats");
    mm_print_stats();
    klog_system_logo();
    shell_run();
    panic("Kernel loop ended!");
}
