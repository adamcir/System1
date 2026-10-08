#include "platform.h"

static const system_platform_ops_t* active_platform;

int platform_register(const system_platform_ops_t* ops) {
    if (!ops || ops->abi_version != SYSTEM_PLATFORM_ABI ||
        !ops->idle || !ops->halt_forever || !ops->reset || !ops->power_off) {
        return -1;
    }
    active_platform = ops;
    return 0;
}

void platform_cpu_idle(void) {
    if (active_platform) active_platform->idle();
}

__attribute__((noreturn)) void platform_halt_forever(void) {
    if (active_platform) active_platform->halt_forever();
    for (;;) {}
}

__attribute__((noreturn)) void platform_reset(void) {
    if (active_platform) active_platform->reset();
    platform_halt_forever();
}

__attribute__((noreturn)) void platform_power_off(void) {
    if (active_platform) active_platform->power_off();
    platform_halt_forever();
}
