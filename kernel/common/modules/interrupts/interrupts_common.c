#include "types.h"
#include "interrupts_common.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI 0x20

#define PIT_CMD      0x43
#define PIT_CH0_DATA 0x40
#define PIT_HZ       100u
#define PIT_BASE_HZ  1193182u

static void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void io_wait(void) {
    outb(0x80, 0);
}

static void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    outb(PIC1_CMD, PIC_EOI);
}

static void pic_set_mask(uint8_t irq, uint8_t masked) {
    uint16_t port;
    uint8_t bit;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
        bit = irq;
    } else {
        port = PIC2_DATA;
        bit = (uint8_t)(irq - 8);
    }

    value = inb(port);
    if (masked) {
        value = (uint8_t)(value | (uint8_t)(1u << bit));
    } else {
        value = (uint8_t)(value & (uint8_t)~(1u << bit));
    }
    outb(port, value);
}

void interrupts_common_state_reset(interrupts_common_state_t* state) {
    uint8_t i;

    for (i = 0; i < IRQ_COUNT; i++) {
        state->irq_handlers[i] = 0;
    }

    state->timer_ticks = 0;
}

void interrupts_common_pic_remap(uint8_t offset1, uint8_t offset2) {
    uint8_t master_mask = inb(PIC1_DATA);
    uint8_t slave_mask = inb(PIC2_DATA);

    outb(PIC1_CMD, 0x11);
    io_wait();
    outb(PIC2_CMD, 0x11);
    io_wait();

    outb(PIC1_DATA, offset1);
    io_wait();
    outb(PIC2_DATA, offset2);
    io_wait();

    outb(PIC1_DATA, 4);
    io_wait();
    outb(PIC2_DATA, 2);
    io_wait();

    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    outb(PIC1_DATA, master_mask);
    outb(PIC2_DATA, slave_mask);
}

void interrupts_common_pic_set_default_masks(void) {
    outb(PIC1_DATA, 0xF8);
    outb(PIC2_DATA, 0xFF);
}

void interrupts_common_pit_init(uint32_t hz) {
    uint16_t divisor;

    if (hz == 0) {
        hz = PIT_HZ;
    }

    divisor = (uint16_t)(PIT_BASE_HZ / hz);
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0_DATA, (uint8_t)(divisor & 0xFFu));
    outb(PIT_CH0_DATA, (uint8_t)((divisor >> 8) & 0xFFu));
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

    pic_send_eoi(irq);
}

int interrupts_common_irq_register_handler(interrupts_common_state_t* state, uint8_t irq, interrupts_common_irq_handler_t handler) {
    uint8_t i;
    uint8_t has_slave_handler = 0;

    if (irq >= IRQ_COUNT) {
        return -1;
    }

    state->irq_handlers[irq] = handler;

    if (handler != 0) {
        pic_set_mask(irq, 0);
        if (irq >= 8) {
            pic_set_mask(2, 0);
        }
        return 0;
    }

    pic_set_mask(irq, 1);
    if (irq >= 8) {
        for (i = 8; i < IRQ_COUNT; i++) {
            if (state->irq_handlers[i] != 0) {
                has_slave_handler = 1;
                break;
            }
        }
        if (has_slave_handler == 0) {
            pic_set_mask(2, 1);
        }
    }

    return 0;
}

uint64_t interrupts_common_timer_ticks_get_raw(const interrupts_common_state_t* state) {
    return state->timer_ticks;
}
