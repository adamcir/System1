#include "signals.h"
#include "signals_core.h"

void signal_raise(int signal) {
    signal_core_raise(signal);
}
