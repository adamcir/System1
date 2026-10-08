#include "platform.h"

/* IBM PC-compatible CPU idle/reboot and ACPI/QEMU poweroff bootstrap.
 * A MULTIPLAN/1 hardware port may use a different implementation.
 */
static uint8_t pc_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static void pc_outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
static void pc_outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static void pc_cpu_idle(void) {
    __asm__ volatile ("hlt");
}

static __attribute__((noreturn)) void pc_halt_forever(void) {
    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static __attribute__((noreturn)) void pc_reset(void) {
    uint32_t timeout;
    __asm__ volatile ("cli");
    for (timeout = 0u; timeout < 1000000u; ++timeout) {
        if ((pc_inb(0x64u) & 0x02u) == 0u) break;
    }
    if (timeout != 1000000u) pc_outb(0x64u, 0xFEu);
    pc_halt_forever();
}

static __attribute__((noreturn)) void pc_power_off(void) {
    __asm__ volatile ("cli");
    pc_outw(0x604u, 0x2000u);
    pc_outw(0xB004u, 0x2000u);
    pc_outw(0x4004u, 0x3400u);
    pc_halt_forever();
}

static const system_platform_ops_t pc_ops = {
    SYSTEM_PLATFORM_ABI,
    pc_cpu_idle,
    pc_halt_forever,
    pc_reset,
    pc_power_off
};

void platform_early_init(void) {
    (void)platform_register(&pc_ops);
}
