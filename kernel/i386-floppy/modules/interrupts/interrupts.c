#include "interrupts.h"

void interrupts_init(void) {
    /* Placeholder: floppy branch uses same future IDT/IRQ milestone. */
}

void interrupts_enable(void) {
    __asm__ volatile ("sti");
}

void interrupts_disable(void) {
    __asm__ volatile ("cli");
}
