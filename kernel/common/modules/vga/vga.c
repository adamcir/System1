#include "types.h"
#include "vga.h"
#include "vga_core.h"

void vga_init(void) {
    vga_core_init();
}

void vga_set_color(vga_color_t new_color) {
    vga_core_set_color((uint8_t)new_color);
}

void vga_set_cursor(unsigned short new_row, unsigned short new_col) {
    vga_core_set_cursor((uint16_t)new_row, (uint16_t)new_col);
}

void vga_get_cursor(unsigned short* out_row, unsigned short* out_col) {
    vga_core_get_cursor((uint16_t*)out_row, (uint16_t*)out_col);
}

void vga_putc_at(unsigned short at_row, unsigned short at_col, char c) {
    vga_core_putc_at((uint16_t)at_row, (uint16_t)at_col, c);
}

char vga_getc_at(unsigned short at_row, unsigned short at_col) {
    return vga_core_getc_at((uint16_t)at_row, (uint16_t)at_col);
}

void vga_putc(char c) {
    vga_core_putc(c);
}

void vga_puts(const char* s) {
    vga_core_puts(s);
}

void vga_hex_u32(unsigned int value) {
    vga_core_hex_u32((uint32_t)value);
}

void vga_text_begin(unsigned short row, unsigned short col) {
    vga_core_text_begin((uint16_t)row, (uint16_t)col);
}

void vga_text_putc(char c) {
    vga_core_text_putc(c);
}

void vga_text_backspace(void) {
    vga_core_text_backspace();
}

void vga_text_left(void) {
    vga_core_text_left();
}

void vga_text_right(void) {
    vga_core_text_right();
}

void vga_text_delete(void) {
    vga_core_text_delete();
}

void vga_text_home(void) {
    vga_core_text_home();
}

void vga_text_end(void) {
    vga_core_text_end();
}

void vga_text_toggle_insert(void) {
    vga_core_text_toggle_insert();
}
