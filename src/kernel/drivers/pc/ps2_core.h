#ifndef SYSTEM_PC_PS2_CORE_H
#define SYSTEM_PC_PS2_CORE_H

#include "input.h"

void ps2_init(void);
void ps2_set_poll_fallback(uint8_t enabled);
void ps2_poll(void);
void ps2_irq_handler(void);
char ps2_last_char(void);
char ps2_take_char(void);
int ps2_take_key(void);

#endif
