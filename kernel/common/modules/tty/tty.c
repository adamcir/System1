#include "tty.h"
#include "tty-core.h"

int tty_readline(char* buf, uint32_t cap) {
    return tty_core_readline(buf, cap);
}

int tty_readline_ex(char* buf, uint32_t cap, const tty_readline_hooks_t* hooks) {
    return tty_core_readline_ex(buf, cap, hooks);
}
