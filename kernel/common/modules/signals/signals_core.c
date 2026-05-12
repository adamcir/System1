#include "types.h"
#include "signals.h"
#include "signals_core.h"

static uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static __attribute__((noreturn)) void halt(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static __attribute__((noreturn)) void hw_reset(void) {
    __asm__ volatile ("cli");
    while (inb(0x64) & 0x02u) {
    }
    outb(0x64, 0xFEu);
    halt();
}

static __attribute__((noreturn)) void hw_power_down(void) {
    __asm__ volatile ("cli");

    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    halt();
}

void signal_core_raise(int signal) {
    if (signal == HW_RESET) {
        hw_reset();
    }

    if (signal == HW_PWR_DOWN) {
        hw_power_down();
    }
}
