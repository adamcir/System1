#include "bootlog.h"
#include "vga.h"

void bootlog_info(const char* msg) {
    vga_puts("[boot] ");
    vga_puts(msg);
    vga_puts("\n");
}
