#ifndef SYSTEM1_FLP_KEYBOARD_H
#define SYSTEM1_FLP_KEYBOARD_H

#define KEY_NONE 0
#define KEY_LEFT 0x80
#define KEY_RIGHT 0x81

void keyboard_init(void);
void keyboard_poll(void);
char keyboard_last_char(void);
char keyboard_take_char(void);
int keyboard_take_key(void);

#endif
