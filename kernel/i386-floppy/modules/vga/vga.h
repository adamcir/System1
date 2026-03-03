#ifndef SYSTEM1_FLP_VGA_H
#define SYSTEM1_FLP_VGA_H

void vga_init(void);
void vga_putc(char c);
void vga_puts(const char* s);
void vga_hex_u32(unsigned int value);

#endif
