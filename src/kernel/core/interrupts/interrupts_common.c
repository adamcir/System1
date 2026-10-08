#include "types.h"
#include "interrupts_common.h"
#include "irq_chip.h"


void interrupts_common_state_reset(interrupts_common_state_t* state) {
    uint8_t i;

    for (i = 0; i < IRQ_COUNT; i++) {
        state->irq_handlers[i] = 0;
    }

    state->timer_ticks = 0;
}

void interrupts_common_pic_remap(uint8_t first, uint8_t second) {
    irq_chip_remap(first, second);
}

void interrupts_common_pic_set_default_masks(void) {
    irq_chip_set_default_masks();
}

void interrupts_common_pit_init(uint32_t hz) {
    irq_chip_init_timer(hz);
}

void interrupts_common_timer_irq(interrupts_common_state_t* state) {
    state->timer_ticks++;
}

void interrupts_common_dispatch(interrupts_common_state_t* state, uint8_t vector) {
    uint8_t irq;
    interrupts_common_irq_handler_t handler;

    if (vector < 32 || vector >= 48) {
        return;
    }

    irq = (uint8_t)(vector - 32);
    handler = state->irq_handlers[irq];
    if (handler != 0) {
        handler();
    }

    irq_chip_send_eoi(irq);
}

int interrupts_common_irq_register_handler(interrupts_common_state_t* state, uint8_t irq, interrupts_common_irq_handler_t handler) {
    uint8_t i;
    uint8_t has_slave_handler = 0;

    if (irq >= IRQ_COUNT) {
        return -1;
    }

    state->irq_handlers[irq] = handler;

    if (handler != 0) {
        irq_chip_mask_irq(irq, 0);
        if (irq >= 8) {
            irq_chip_mask_irq(2, 0);
        }
        return 0;
    }

    irq_chip_mask_irq(irq, 1);
    if (irq >= 8) {
        for (i = 8; i < IRQ_COUNT; i++) {
            if (state->irq_handlers[i] != 0) {
                has_slave_handler = 1;
                break;
            }
        }
        if (has_slave_handler == 0) {
            irq_chip_mask_irq(2, 1);
        }
    }

    return 0;
}

uint64_t interrupts_common_timer_ticks_get_raw(const interrupts_common_state_t* state) {
    return state->timer_ticks;
}
