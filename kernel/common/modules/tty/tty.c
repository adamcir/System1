#include "tty.h"
#include "tty-core.h"

void tty_run(void) {
    tty_core_run();
}

int tty_readline(char* buf, uint32_t cap) {
    return tty_core_readline(buf, cap);
}
