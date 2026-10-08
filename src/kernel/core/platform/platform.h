#ifndef SYSTEM_PLATFORM_H
#define SYSTEM_PLATFORM_H

#include "types.h"

/* System platform ABI for essential CPU and power operations.
 * The boot loader/kernel sets this up before the first TTY output.
 * No hard-coded PC instructions or I/O ports belong in the kernel services.
 */
#define SYSTEM_PLATFORM_ABI 1u

typedef struct {
    uint32_t abi_version;
    void (*idle)(void);
    __attribute__((noreturn)) void (*halt_forever)(void);
    __attribute__((noreturn)) void (*reset)(void);
    __attribute__((noreturn)) void (*power_off)(void);
} system_platform_ops_t;

int platform_register(const system_platform_ops_t* ops);
void platform_early_init(void);
void platform_cpu_idle(void);
__attribute__((noreturn)) void platform_halt_forever(void);
__attribute__((noreturn)) void platform_reset(void);
__attribute__((noreturn)) void platform_power_off(void);

#endif
