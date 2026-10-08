#include "display.h"

static const system_display_ops_t* active_display;

int display_register(const system_display_ops_t* ops) {
    if (!ops || ops->abi != SYSTEM_DISPLAY_ABI ||
        !ops->init || !ops->set_color || !ops->get_cursor ||
        !ops->putc || !ops->puts || !ops->hex_u32 ||
        !ops->text_begin || !ops->text_putc ||
        !ops->text_backspace || !ops->text_left ||
        !ops->text_right || !ops->text_delete ||
        !ops->text_toggle_insert) return -1;
    active_display = ops;
    return 0;
}
void display_init(void) { if (active_display) active_display->init(); }
void display_set_color(uint8_t c) { if (active_display) active_display->set_color(c); }
void display_get_cursor(uint16_t* r, uint16_t* c) {
    if (active_display) active_display->get_cursor(r,c);
    else { if (r) *r=0; if (c) *c=0; }
}
void display_putc(char c) { if (active_display) active_display->putc(c); }
void display_puts(const char* s) { if (active_display) active_display->puts(s); }
void display_hex_u32(uint32_t n) { if (active_display) active_display->hex_u32(n); }
void display_text_begin(uint16_t r,uint16_t c) { if (active_display) active_display->text_begin(r,c); }
void display_text_putc(char c) { if (active_display) active_display->text_putc(c); }
void display_text_backspace(void) { if (active_display) active_display->text_backspace(); }
void display_text_left(void) { if (active_display) active_display->text_left(); }
void display_text_right(void) { if (active_display) active_display->text_right(); }
void display_text_delete(void) { if (active_display) active_display->text_delete(); }
void display_text_toggle_insert(void) { if (active_display) active_display->text_toggle_insert(); }
