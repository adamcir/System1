#include "panic_core.h"
#include "vga.h"

static __attribute__((noreturn)) void panic_halt(void) {
	__asm__ volatile ("cli");
	for (;;) {
		__asm__ volatile ("hlt");
	}
}

__attribute__((noreturn)) void panic_core(const char* msg) {
	vga_set_color(RED);
    vga_puts("[PANIC! : ");
    vga_puts(msg);
    vga_putc(']');
    vga_puts("\n");
	panic_halt();
}
