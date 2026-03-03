#ifndef SYSTEM1_X64_INTERRUPTS_H
#define SYSTEM1_X64_INTERRUPTS_H

#include "types.h"

typedef void (*irq_handler_t)(void);

void interrupts_init(void);
void interrupts_enable(void);
void interrupts_disable(void);
int irq_register_handler(uint8_t irq, irq_handler_t handler);
uint64_t timer_ticks_get(void);

#endif
