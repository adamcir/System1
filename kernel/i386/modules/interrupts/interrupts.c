#include "interrupts.h"

void interrupts_init(void) {
    /* Placeholder: full IDT/IRQ wiring comes in next milestone. */
}

void interrupts_enable(void) {
    __asm__ volatile ("sti");
}

void interrupts_disable(void) {
    __asm__ volatile ("cli");
}
