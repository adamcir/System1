#include "types.h"
#include "klog.h"
#include "interrupts.h"
#include "irq_chip.h"
#include "input.h"
#include "shell.h"
#include "tty.h"
#include "display.h"
#include "platform.h"
#include "paging.h"
#include "mm.h"
#include "bootstrap.h"
#include "module.h"
#include "usermode.h"

void kmain_i386(uint32_t magic, uint32_t info) {
    platform_early_init();

    if (paging_init(magic, info) != 0) {
        panic("Paging_init failed");
    }
    klog_info("paging", "Initialized");

    (void)smod_boot_early_init(magic, info);
    tty_init();

    if (mm_init(magic, info) != 0) {
        panic("MM_init failed");
    }
    klog_info("mm", "Initialized");

    if (usermode_init() != 0) {
        panic("Usermode_init failed");
    }
    klog_info("usermode", "Initialized");

    irq_chip_platform_init();
    interrupts_init();
    input_init();
    irq_register_handler(1, input_irq_handler);
    interrupts_enable();
    klog_info("boot", "System/1 boot via GRUB");
    if (bootstrap_init(magic, info) != 0) {
        panic("Bootstrap failed");
    }
    (void)bootstrap_start_shell();
    shell_run();
    panic("Kernel loop ended!");
}
