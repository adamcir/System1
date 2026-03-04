#ifndef SYSTEM1_I386_FLOPPY_KEYBOARD_H
#define SYSTEM1_I386_FLOPPY_KEYBOARD_H

#include "types.h"

#define KEY_NONE 0
#define KEY_LEFT 0x80
#define KEY_RIGHT 0x81
#define KEY_INSERT 0x82
#define KEY_DELETE 0x83
#define KEY_HOME 0x84
#define KEY_END 0x85
#define KEY_CTRL_ALT_DEL 0x86
#define KEY_CTRL_ALT_BKSP 0x87

void keyboard_init(void);
void keyboard_set_poll_fallback(uint8_t enabled);
void keyboard_poll(void);
void keyboard_irq_handler(void);
char keyboard_last_char(void);
char keyboard_take_char(void);
int keyboard_take_key(void);

#endif
