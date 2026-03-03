#ifndef SYSTEM1_I386_VGA_H
#define SYSTEM1_I386_VGA_H

void vga_init(void);
void vga_putc(char c);
void vga_puts(const char* s);

#endif
