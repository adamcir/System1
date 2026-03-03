#include "bootlog_core.h"

extern void vga_puts(const char* s);

void bootlog_info_core(const char* prefix, const char* msg) {
    vga_puts(prefix);
    vga_puts(msg);
    vga_puts("\n");
}
