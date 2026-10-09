#ifndef SYSTEM_IRQ_CHIP_H
#define SYSTEM_IRQ_CHIP_H

#include "types.h"
#define SYSTEM_IRQ_CHIP_ABI 1u

/* Architecture/platform-specific IRQ controller and timer operations.
 * The generic interrupt dispatcher must never access PC I/O ports.
 */
typedef struct system_irq_chip_ops {
    uint32_t abi;
    void (*remap)(uint8_t first, uint8_t second);
    void (*set_default_masks)(void);
    void (*mask_irq)(uint8_t irq, uint8_t masked);
    void (*send_eoi)(uint8_t irq);
    void (*init_timer)(uint32_t hz);
} system_irq_chip_ops_t;

int irq_chip_has_driver(void);
int irq_chip_register(const system_irq_chip_ops_t* ops);
void irq_chip_platform_init(void); /* Built-in PC boot driver */
void irq_chip_remap(uint8_t first, uint8_t second);
void irq_chip_set_default_masks(void);
void irq_chip_mask_irq(uint8_t irq, uint8_t masked);
void irq_chip_send_eoi(uint8_t irq);
void irq_chip_init_timer(uint32_t hz);
#endif
