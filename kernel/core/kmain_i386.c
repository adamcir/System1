#include "types.h"
#include "bootlog.h"
#include "vga.h"

void kmain_i386(uint32_t magic, uint32_t info) {
    (void)magic;
    (void)info;

    vga_init();
    bootlog_info("System/1 i386 boot via GRUB");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
