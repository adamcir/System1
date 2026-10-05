#ifndef SYSTEM1_COMMON_INTERRUPTS_COMMON_H
#define SYSTEM1_COMMON_INTERRUPTS_COMMON_H

#include "types.h"

#define IDT_SIZE 256
#define IRQ_COUNT 16

typedef void (*interrupts_common_irq_handler_t)(void);

typedef struct {
    interrupts_common_irq_handler_t irq_handlers[IRQ_COUNT];
    volatile uint64_t timer_ticks;
} interrupts_common_state_t;

void interrupts_common_state_reset(interrupts_common_state_t* state);
void interrupts_common_pic_remap(uint8_t offset1, uint8_t offset2);
void interrupts_common_pic_set_default_masks(void);
void interrupts_common_pit_init(uint32_t hz);
void interrupts_common_timer_irq(interrupts_common_state_t* state);
void interrupts_common_dispatch(interrupts_common_state_t* state, uint8_t vector);
int interrupts_common_irq_register_handler(interrupts_common_state_t* state, uint8_t irq, interrupts_common_irq_handler_t handler);
uint64_t interrupts_common_timer_ticks_get_raw(const interrupts_common_state_t* state);

#endif
