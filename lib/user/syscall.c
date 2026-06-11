#include "system1/errno.h"
#include "system1/unistd.h"

static system1_syscall_handler_t g_syscall_handler = 0;

void system1_set_syscall_handler(system1_syscall_handler_t handler) {
    g_syscall_handler = handler;
}

int system1_syscall(uint32_t nr, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    if (g_syscall_handler == 0) {
        return -ENOSYS;
    }

    return g_syscall_handler(nr, a0, a1, a2, a3);
}
