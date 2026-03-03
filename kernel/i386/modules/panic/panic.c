#include "panic.h"
#include "vga.h"

__attribute__((noreturn)) void panic(const char* msg) {
    vga_puts("PANIC: ");
    vga_puts(msg);
    vga_puts("\n");
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
