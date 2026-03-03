#include "panic_core.h"

extern void vga_puts(const char* s);

__attribute__((noreturn)) void panic_core(const char* msg) {
    vga_puts("PANIC: ");
    vga_puts(msg);
    vga_puts("\n");
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
