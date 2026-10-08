#include "types.h"
#include "klog.h"
#include "interrupts.h"
#include "keyboard.h"
#include "shell.h"
#include "tty.h"
#include "display.h"
#include "paging.h"
#include "mm.h"
#include "bootstrap.h"
#include "usermode.h"

#define FLOPPY_MAGIC 0x53314D47u

void kmain_floppy_i386(uint32_t magic, uint32_t boot_info_ptr) {
    display_platform_init();
    tty_init();
    if (paging_init(magic, boot_info_ptr) != 0) {
        panic("Paging_init failed");
    }
    klog_info("paging", "Initialized");

    if (mm_init(magic, boot_info_ptr) != 0) {
        panic("MM_init failed");
    }
    klog_info("mm", "Initialized");

    if (usermode_init() != 0) {
        panic("Usermode_init failed");
    }
    klog_info("usermode", "Initialized");

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
    if (bootstrap_init(magic, boot_info_ptr) != 0) {
        panic("Bootstrap failed");
    }
    (void)bootstrap_start_shell();
    shell_run();
    panic("Kernel loop ended!");
}
