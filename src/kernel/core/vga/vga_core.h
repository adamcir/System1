#ifndef SYSTEM1_COMMON_VGA_CORE_H
#define SYSTEM1_COMMON_VGA_CORE_H

#include "types.h"

void vga_core_init(void);
void vga_core_set_color(uint8_t color);
void vga_core_set_cursor(uint16_t new_row, uint16_t new_col);
void vga_core_get_cursor(uint16_t* out_row, uint16_t* out_col);
void vga_core_putc_at(uint16_t at_row, uint16_t at_col, char c);
char vga_core_getc_at(uint16_t at_row, uint16_t at_col);
void vga_core_putc(char c);
void vga_core_puts(const char* s);
void vga_core_hex_u32(uint32_t value);
void vga_core_text_begin(uint16_t row, uint16_t col);
void vga_core_text_putc(char c);
void vga_core_text_backspace(void);
void vga_core_text_left(void);
void vga_core_text_right(void);
void vga_core_text_delete(void);
void vga_core_text_toggle_insert(void);

#endif
