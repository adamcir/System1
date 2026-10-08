#include "display.h"
#include "vga_text.h"

/* PC VGA text mode driver; built in until the SMOD loader can boot from disk. */
static const system_display_ops_t pc_vga_display_ops = {
    SYSTEM_DISPLAY_ABI,
    pc_vga_init,
    pc_vga_set_color,
    pc_vga_set_cursor,
    pc_vga_get_cursor,
    pc_vga_putc,
    pc_vga_puts,
    pc_vga_hex_u32,
    pc_vga_text_begin,
    pc_vga_text_putc,
    pc_vga_text_backspace,
    pc_vga_text_left,
    pc_vga_text_right,
    pc_vga_text_delete,
    pc_vga_text_toggle_insert
};
void display_platform_init(void) {
    (void)display_register(&pc_vga_display_ops);
}
