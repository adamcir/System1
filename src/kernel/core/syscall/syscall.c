#include "syscall.h"
#include "syscall_core.h"

void syscall_init(void) {
    syscall_core_init();
}

int syscall_dispatch(uint32_t nr, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    return syscall_core_dispatch(nr, a0, a1, a2, a3);
}
