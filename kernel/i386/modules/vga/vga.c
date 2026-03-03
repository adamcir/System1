#include "types.h"
#include "vga.h"

static volatile uint16_t* const VGA = (uint16_t*)0xB8000;
static uint16_t row;
static uint16_t col;
static uint8_t color = LIGHT_GREY;

static void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void vga_hw_cursor_update(void) {
    uint16_t pos = (uint16_t)(row * 80 + col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_init(void) {
    uint16_t i;
    row = 0;
    col = 0;
    for (i = 0; i < 80 * 25; ++i) {
        VGA[i] = ((uint16_t)color << 8) | ' ';
    }
    vga_hw_cursor_update();
}

void vga_set_color(vga_color_t new_color) {
    color = (uint8_t)new_color;
}

void vga_set_cursor(unsigned short new_row, unsigned short new_col) {
    row = (uint16_t)(new_row % 25);
    col = (uint16_t)(new_col % 80);
    vga_hw_cursor_update();
}

void vga_get_cursor(unsigned short* out_row, unsigned short* out_col) {
    if (out_row != 0) {
        *out_row = row;
    }
    if (out_col != 0) {
        *out_col = col;
    }
}

void vga_putc_at(unsigned short at_row, unsigned short at_col, char c) {
    uint16_t r = (uint16_t)(at_row % 25);
    uint16_t ccol = (uint16_t)(at_col % 80);
    VGA[r * 80 + ccol] = ((uint16_t)color << 8) | (uint8_t)c;
}

void vga_putc(char c) {
    if (c == '\n') {
        col = 0;
        row++;
        if (row >= 25) {
            row = 0;
        }
        vga_hw_cursor_update();
        return;
    }

    VGA[row * 80 + col] = ((uint16_t)color << 8) | (uint8_t)c;
    col++;

    if (col >= 80) {
        col = 0;
        row++;
    }
    if (row >= 25) {
        row = 0;
    }
    vga_hw_cursor_update();
}

void vga_puts(const char* s) {
    while (*s) {
        vga_putc(*s++);
    }
}
