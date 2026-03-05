#include "tty.h"
#include "tty-core.h"

int tty_readline(char* buf, uint32_t cap) {
    return tty_core_readline(buf, cap);
}
