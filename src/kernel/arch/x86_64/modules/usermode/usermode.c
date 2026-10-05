#include "usermode.h"
#include "posix_errno.h"

int usermode_init(void) {
    return 0;
}

int usermode_exec(const char* path) {
    (void)path;
    return -POSIX_ENOSYS;
}

void usermode_exit(int status) {
    (void)status;
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
