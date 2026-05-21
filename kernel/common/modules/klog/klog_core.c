#include "klog_core.h"
#include "vga.h"

static __attribute__((noreturn)) void panic_halt(void) {
	__asm__ volatile ("cli");
	for (;;) {
		__asm__ volatile ("hlt");
	}
}

__attribute__((noreturn)) void panic_core(const char* msg) {
	vga_set_color(RED);
    vga_puts("[PANIC!]: ");
    vga_puts(msg);
    vga_puts("\n");
	panic_halt();
}

void klog_info_core(const char* prefix, const char* msg) {
	vga_set_color(YELLOW);
	vga_putc('[');
    vga_puts(prefix);
    vga_puts("]: ");
    vga_puts(msg);
    vga_puts("\n");
}

void klog_system_logo_core(void) {
	vga_set_color(WHITE);
    vga_puts("==================================================\n");
    vga_puts("   ____            _                     __  __\n");
    vga_puts("  / ___| _   _ ___| |_ ___ _ __ ___     / / /_ |\n");
    vga_puts("  \\___ \\| | | / __| __/ _ \\ '_ ` _ \\   / /   | |\n");
    vga_puts("   ___) | |_| \\__ \\ ||  __/ | | | | | / /    | |\n");
    vga_puts("  |____/ \\__, |___/\\__\\___|_| |_| |_|/ /     |_|\n");
    vga_puts("         |___/                      /_/\n\n");
    vga_puts("==================================================\n");
    vga_puts("=[System/1 by AdavaSoftware (C) 2026]=============\n\n");
}

