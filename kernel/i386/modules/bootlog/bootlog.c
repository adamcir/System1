#include "bootlog.h"
#include "vga.h"

void bootlog_info(const char* msg) {
    vga_puts("[i386] ");
    vga_puts(msg);
    vga_puts("\n");
}
