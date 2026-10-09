#include "smod.h"
#include "display.h"

/* Native hardware VGA text console with cursor + line editing.
 * Limit the module-owned editing buffer to save boot memory on 1 MiB PCs.
 */
#define VGA_TEXT_PATH_MAX 512u
#include "../../kernel/drivers/pc/vga_text.c"

__attribute__((section(".text.smod_entry")))
int smod_entry(const smod_api_v3_t* api) {
    system_display_ops_t driver;
    if (!api || api->abi_version != SMOD_API_VERSION ||
        !api->register_display) return -1;
    driver.abi = SYSTEM_DISPLAY_ABI;
    driver.init = pc_vga_init;
    driver.set_color = pc_vga_set_color;
    driver.set_cursor = pc_vga_set_cursor;
    driver.get_cursor = pc_vga_get_cursor;
    driver.putc = pc_vga_putc;
    driver.puts = pc_vga_puts;
    driver.hex_u32 = pc_vga_hex_u32;
    driver.text_begin = pc_vga_text_begin;
    driver.text_putc = pc_vga_text_putc;
    driver.text_backspace = pc_vga_text_backspace;
    driver.text_left = pc_vga_text_left;
    driver.text_right = pc_vga_text_right;
    driver.text_delete = pc_vga_text_delete;
    driver.text_toggle_insert = pc_vga_text_toggle_insert;
    return api->register_display(&driver);
}
