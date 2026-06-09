#include "klog_core.h"
#include "tty.h"

static __attribute__((noreturn)) void panic_halt(void) {
	__asm__ volatile ("cli");
	for (;;) {
		__asm__ volatile ("hlt");
	}
}

__attribute__((noreturn)) void panic_core(const char* msg) {
	tty_set_color(TTY_RED);
    tty_puts("[PANIC!]: ");
    tty_puts(msg);
    tty_puts("\n");
	panic_halt();
}

void klog_info_core(const char* prefix, const char* msg) {
	tty_set_color(TTY_YELLOW);
	tty_putc('[');
    tty_puts(prefix);
    tty_puts("]: ");
    tty_puts(msg);
    tty_puts("\n");
}

void klog_system_logo_core(void) {
	tty_set_color(TTY_WHITE);
    tty_puts("==================================================\n");
    tty_puts("   ____            _                     __  __\n");
    tty_puts("  / ___| _   _ ___| |_ ___ _ __ ___     / / /_ |\n");
    tty_puts("  \\___ \\| | | / __| __/ _ \\ '_ ` _ \\   / /   | |\n");
    tty_puts("   ___) | |_| \\__ \\ ||  __/ | | | | | / /    | |\n");
    tty_puts("  |____/ \\__, |___/\\__\\___|_| |_| |_|/ /     |_|\n");
    tty_puts("         |___/                      /_/\n\n");
    tty_puts("==================================================\n");
    tty_puts("=[System/1 by AdavaSoftware (C) 2026]=============\n\n");
}
