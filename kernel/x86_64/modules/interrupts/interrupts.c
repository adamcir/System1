#include "interrupts.h"
#include "interrupts_common.h"
#include "paging.h"

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} idt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} idtr_t;

static idt_entry_t g_idt[IDT_SIZE];
static interrupts_common_state_t g_interrupts_state;

#define DECL_ISR(n) extern void isr##n(void)
DECL_ISR(0);  DECL_ISR(1);  DECL_ISR(2);  DECL_ISR(3);
DECL_ISR(4);  DECL_ISR(5);  DECL_ISR(6);  DECL_ISR(7);
DECL_ISR(8);  DECL_ISR(9);  DECL_ISR(10); DECL_ISR(11);
DECL_ISR(12); DECL_ISR(13); DECL_ISR(14); DECL_ISR(15);
DECL_ISR(16); DECL_ISR(17); DECL_ISR(18); DECL_ISR(19);
DECL_ISR(20); DECL_ISR(21); DECL_ISR(22); DECL_ISR(23);
DECL_ISR(24); DECL_ISR(25); DECL_ISR(26); DECL_ISR(27);
DECL_ISR(28); DECL_ISR(29); DECL_ISR(30); DECL_ISR(31);
DECL_ISR(32); DECL_ISR(33); DECL_ISR(34); DECL_ISR(35);
DECL_ISR(36); DECL_ISR(37); DECL_ISR(38); DECL_ISR(39);
DECL_ISR(40); DECL_ISR(41); DECL_ISR(42); DECL_ISR(43);
DECL_ISR(44); DECL_ISR(45); DECL_ISR(46); DECL_ISR(47);
#undef DECL_ISR

static void (*const g_isr_stub_table[48])(void) = {
    isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
    isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
    isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39,
    isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47
};

static void lidt(const idtr_t* idtr) {
    __asm__ volatile ("lidt (%0)" : : "r"(idtr));
}

static uint16_t read_cs(void) {
    uint16_t cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    return cs;
}

static void idt_set_gate(uint8_t vector, uint64_t handler_addr, uint16_t selector, uint8_t type_attr) {
    g_idt[vector].offset_low = (uint16_t)(handler_addr & 0xFFFFu);
    g_idt[vector].selector = selector;
    g_idt[vector].ist = 0;
    g_idt[vector].type_attr = type_attr;
    g_idt[vector].offset_mid = (uint16_t)((handler_addr >> 16) & 0xFFFFu);
    g_idt[vector].offset_high = (uint32_t)((handler_addr >> 32) & 0xFFFFFFFFu);
    g_idt[vector].zero = 0;
}

static void timer_irq_handler(void) {
    interrupts_common_timer_irq(&g_interrupts_state);
}

void interrupts_dispatch(uint8_t vector) {
    if (vector == 14u) {
        paging_handle_page_fault();
        return;
    }
    interrupts_common_dispatch(&g_interrupts_state, vector);
}

void interrupts_init(void) {
    idtr_t idtr;
    uint16_t code_selector;
    uint16_t i;

    interrupts_disable();

    for (i = 0; i < IDT_SIZE; i++) {
        g_idt[i].offset_low = 0;
        g_idt[i].selector = 0;
        g_idt[i].ist = 0;
        g_idt[i].type_attr = 0;
        g_idt[i].offset_mid = 0;
        g_idt[i].offset_high = 0;
        g_idt[i].zero = 0;
    }

    code_selector = read_cs();
    for (i = 0; i < 48; i++) {
        idt_set_gate((uint8_t)i, (uint64_t)(uintptr_t)g_isr_stub_table[i], code_selector, 0x8E);
    }

    idtr.limit = (uint16_t)(sizeof(g_idt) - 1);
    idtr.base = (uint64_t)(uintptr_t)&g_idt[0];
    lidt(&idtr);

    interrupts_common_state_reset(&g_interrupts_state);
    interrupts_common_pic_remap(0x20, 0x28);
    interrupts_common_pic_set_default_masks();
    interrupts_common_pit_init(100u);
    irq_register_handler(0, timer_irq_handler);
}

void interrupts_enable(void) {
    __asm__ volatile ("sti");
}

void interrupts_disable(void) {
    __asm__ volatile ("cli");
}

int irq_register_handler(uint8_t irq, irq_handler_t handler) {
    return interrupts_common_irq_register_handler(&g_interrupts_state, irq, handler);
}

uint64_t timer_ticks_get(void) {
    uint64_t flags;
    uint64_t ticks;

    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    ticks = interrupts_common_timer_ticks_get_raw(&g_interrupts_state);
    if ((flags & (1ull << 9)) != 0ull) {
        __asm__ volatile ("sti" : : : "memory");
    }

    return ticks;
}
