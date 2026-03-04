#ifndef SYSTEM1_FLP_VGA_H
#define SYSTEM1_FLP_VGA_H

/* VGA barvy: BARVA > cislo */
#define VGA_COLOR_BLACK 0
#define VGA_COLOR_BLUE 1
#define VGA_COLOR_GREEN 2
#define VGA_COLOR_CYAN 3
#define VGA_COLOR_RED 4
#define VGA_COLOR_MAGENTA 5
#define VGA_COLOR_BROWN 6
#define VGA_COLOR_LIGHT_GREY 7
#define VGA_COLOR_DARK_GREY 8
#define VGA_COLOR_LIGHT_BLUE 9
#define VGA_COLOR_LIGHT_GREEN 10
#define VGA_COLOR_LIGHT_CYAN 11
#define VGA_COLOR_LIGHT_RED 12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_LIGHT_BROWN 14
#define VGA_COLOR_YELLOW VGA_COLOR_LIGHT_BROWN
#define VGA_COLOR_WHITE 15

typedef enum {
    BLACK = VGA_COLOR_BLACK,
    BLUE = VGA_COLOR_BLUE,
    GREEN = VGA_COLOR_GREEN,
    CYAN = VGA_COLOR_CYAN,
    RED = VGA_COLOR_RED,
    MAGENTA = VGA_COLOR_MAGENTA,
    BROWN = VGA_COLOR_BROWN,
    LIGHT_GREY = VGA_COLOR_LIGHT_GREY,
    DARK_GREY = VGA_COLOR_DARK_GREY,
    LIGHT_BLUE = VGA_COLOR_LIGHT_BLUE,
    LIGHT_GREEN = VGA_COLOR_LIGHT_GREEN,
    LIGHT_CYAN = VGA_COLOR_LIGHT_CYAN,
    LIGHT_RED = VGA_COLOR_LIGHT_RED,
    LIGHT_MAGENTA = VGA_COLOR_LIGHT_MAGENTA,
    LIGHT_BROWN = VGA_COLOR_LIGHT_BROWN,
    YELLOW = VGA_COLOR_YELLOW,
    WHITE = VGA_COLOR_WHITE
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
void vga_text_toggle_insert(void);

#endif
