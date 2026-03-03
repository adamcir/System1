#ifndef SYSTEM1_I386_VGA_H
#define SYSTEM1_I386_VGA_H

typedef enum {
    BLACK = 0,
    BLUE = 1,
    GREEN = 2,
    CYAN = 3,
    RED = 4,
    MAGENTA = 5,
    BROWN = 6,
    LIGHT_GREY = 7,
    DARK_GREY = 8,
    LIGHT_BLUE = 9,
    LIGHT_GREEN = 10,
    LIGHT_CYAN = 11,
    LIGHT_RED = 12,
    LIGHT_MAGENTA = 13,
    LIGHT_BROWN = 14,
    WHITE = 15
} vga_color_t;

void vga_init(void);
void vga_set_color(vga_color_t color);
void vga_set_cursor(unsigned short new_row, unsigned short new_col);
void vga_get_cursor(unsigned short* out_row, unsigned short* out_col);
void vga_putc_at(unsigned short at_row, unsigned short at_col, char c);
char vga_getc_at(unsigned short at_row, unsigned short at_col);
void vga_putc(char c);
void vga_puts(const char* s);
void vga_hex_u32(unsigned int value);
void vga_text_begin(unsigned short row, unsigned short col);
void vga_text_putc(char c);
void vga_text_backspace(void);
void vga_text_left(void);
void vga_text_right(void);
void vga_text_delete(void);
void vga_text_home(void);
void vga_text_end(void);
void vga_text_toggle_insert(void);

#endif
