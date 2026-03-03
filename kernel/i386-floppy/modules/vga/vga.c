#include "types.h"
#include "vga.h"

static volatile uint16_t* const VGA = (uint16_t*)0xB8000;
static uint16_t row;
static uint16_t col;
static const uint8_t color = 0x0E;

void vga_init(void) {
    uint16_t i;
    row = 0;
    col = 0;
    for (i = 0; i < 80 * 25; ++i) {
        VGA[i] = ((uint16_t)color << 8) | ' ';
    }
}

void vga_putc(char c) {
    if (c == '\n') {
        col = 0;
        row++;
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
}

void vga_puts(const char* s) {
    while (*s) {
        vga_putc(*s++);
    }
}

void vga_hex_u32(unsigned int value) {
    static const char hex[] = "0123456789ABCDEF";
    int i;
    vga_puts("0x");
    for (i = 7; i >= 0; --i) {
        vga_putc(hex[(value >> (i * 4)) & 0xF]);
    }
}
