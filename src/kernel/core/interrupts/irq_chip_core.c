#include "irq_chip.h"

static const system_irq_chip_ops_t* active_irq_chip;

int irq_chip_register(const system_irq_chip_ops_t* ops) {
    if (!ops || ops->abi != SYSTEM_IRQ_CHIP_ABI || !ops->remap ||
        !ops->set_default_masks || !ops->mask_irq || !ops->send_eoi ||
        !ops->init_timer) return -1;
    active_irq_chip = ops;
    return 0;
}

void irq_chip_remap(uint8_t first, uint8_t second) {
    if (active_irq_chip) active_irq_chip->remap(first, second);
}

void irq_chip_set_default_masks(void) {
    if (active_irq_chip) active_irq_chip->set_default_masks();
}

void irq_chip_mask_irq(uint8_t irq, uint8_t masked) {
    if (active_irq_chip) active_irq_chip->mask_irq(irq, masked);
}

void irq_chip_send_eoi(uint8_t irq) {
    if (active_irq_chip) active_irq_chip->send_eoi(irq);
}

void irq_chip_init_timer(uint32_t hz) {
    if (active_irq_chip) active_irq_chip->init_timer(hz);
}
