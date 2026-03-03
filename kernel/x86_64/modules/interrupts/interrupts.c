#include "interrupts.h"

void interrupts_init(void) {
    /* Placeholder: x86_64 IDT/IRQ wiring is pending. */
}

void interrupts_enable(void) {
    __asm__ volatile ("sti");
}

void interrupts_disable(void) {
    __asm__ volatile ("cli");
}
