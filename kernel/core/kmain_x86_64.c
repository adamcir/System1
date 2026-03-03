#include "types.h"
#include "bootlog.h"
#include "vga.h"

void kmain_x86_64(uint32_t magic, uint32_t info) {
    (void)magic;
    (void)info;

    vga_init();
    bootlog_info("System/1 x86_64 boot via GRUB");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
