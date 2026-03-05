#include "types.h"
#include "klog.h"
#include "interrupts.h"
#include "keyboard.h"
#include "shell.h"
#include "vga.h"
#include "panic.h"

void kmain_i386(uint32_t magic, uint32_t info) {
    (void)magic;
    (void)info;

    vga_init();
    interrupts_init();
    keyboard_init();
    irq_register_handler(1, keyboard_irq_handler);
    interrupts_enable();
    klog_info("boot", "System/1 boot via GRUB");
    klog_info("kernel", "modules: panic, vga, signals, interrupts, keyboard, shell");
    klog_system_logo();
    shell_run();
    panic("Kernel loop ended!");
}
