#include "bootlog_core.h"
#include "vga.h"

void bootlog_info_core(const char* prefix, const char* msg) {
	vga_set_color(YELLOW);
	vga_putc('[');
    vga_puts(prefix);
    vga_puts("]: ");
    vga_puts(msg);
    vga_puts("\n");
}
