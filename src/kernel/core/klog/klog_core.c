#include "klog_core.h"
#include "interrupts.h"
#include "tty.h"
#include "types.h"
#include "platform.h"

static void klog_put_u32_dec(uint32_t value) {
    char digits[10];
    uint32_t count = 0u;

    if (value == 0u) {
        tty_putc('0');
        return;
    }

    while (value != 0u && count < 10u) {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (count > 0u) {
        tty_putc(digits[--count]);
    }
}

static void klog_timestamp(void) {
    uint32_t ticks = (uint32_t)timer_ticks_get();
    uint32_t seconds = ticks / 100u;
    uint32_t centiseconds = ticks % 100u;

    tty_putc('[');
    klog_put_u32_dec(seconds);
    tty_putc('.');
    if (centiseconds < 10u) tty_putc('0');
    klog_put_u32_dec(centiseconds);
    tty_puts("] ");
}

static __attribute__((noreturn)) void panic_halt(void) {
    platform_halt_forever();
}

__attribute__((noreturn)) void panic_core(const char* msg) {
    tty_set_color(TTY_RED);
    klog_timestamp();
    tty_puts("PANIC kernel: ");
    tty_puts(msg);
    tty_putc('\n');
    panic_halt();
}

void klog_info_core(const char* prefix, const char* msg) {
    tty_set_color(TTY_YELLOW);
    klog_timestamp();
    tty_puts("INFO  ");
    tty_puts(prefix);
    tty_puts(": ");
    tty_puts(msg);
    tty_putc('\n');
}
