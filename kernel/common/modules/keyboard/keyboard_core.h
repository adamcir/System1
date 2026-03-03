#ifndef SYSTEM1_COMMON_KEYBOARD_CORE_H
#define SYSTEM1_COMMON_KEYBOARD_CORE_H

#include "types.h"

#define KEY_NONE 0
#define KEY_LEFT 0x80
#define KEY_RIGHT 0x81
#define KEY_INSERT 0x82
#define KEY_DELETE 0x83
#define KEY_HOME 0x84
#define KEY_END 0x85

void keyboard_core_init(void);
void keyboard_core_set_poll_fallback(uint8_t enabled);
void keyboard_core_poll(void);
void keyboard_core_irq_handler(void);
char keyboard_core_last_char(void);
char keyboard_core_take_char(void);
int keyboard_core_take_key(void);

#endif
