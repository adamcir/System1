#ifndef SYSTEM_PC_VGA_TEXT_H
#define SYSTEM_PC_VGA_TEXT_H

#include "types.h"

void pc_vga_init(void);
void pc_vga_set_color(uint8_t color);
void pc_vga_set_cursor(uint16_t new_row, uint16_t new_col);
void pc_vga_get_cursor(uint16_t* out_row, uint16_t* out_col);
void pc_vga_putc_at(uint16_t at_row, uint16_t at_col, char c);
char pc_vga_getc_at(uint16_t at_row, uint16_t at_col);
void pc_vga_putc(char c);
void pc_vga_puts(const char* s);
void pc_vga_hex_u32(uint32_t value);
void pc_vga_text_begin(uint16_t row, uint16_t col);
void pc_vga_text_putc(char c);
void pc_vga_text_backspace(void);
void pc_vga_text_left(void);
void pc_vga_text_right(void);
void pc_vga_text_delete(void);
void pc_vga_text_toggle_insert(void);

#endif
