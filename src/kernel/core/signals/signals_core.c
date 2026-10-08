#include "signals.h"
#include "signals_core.h"
#include "platform.h"

void signal_core_raise(int signal) {
    if (signal == HW_RESET) {
        platform_reset();
    }
    if (signal == HW_PWR_DOWN) {
        platform_power_off();
    }
}
