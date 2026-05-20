#include "types.h"
#include "klog.h"
#include "interrupts.h"
#include "keyboard.h"
#include "shell.h"
#include "vga.h"
#include "paging.h"
#include "fs.h"

void kmain_x86_64(uint32_t magic, uint32_t info) {
    vga_init();
    if (paging_init(magic, info) != 0) {
        panic("paging_init failed");
    }
    interrupts_init();
    keyboard_init();
    irq_register_handler(1, keyboard_irq_handler);
    interrupts_enable();
    klog_info("boot", "System/1 boot via GRUB");
    klog_info("kernel", "modules: vga, signals, interrupts, keyboard, shell");
    fs_set_boot_context(magic, info);
    if (fs_init() != FS_OK) {
        panic("Unable to mount FS");
    }
    klog_system_logo();
    shell_run();
    panic("Kernel loop ended!");
}
