#include "usermode.h"
#include "posix_errno.h"

int usermode_init(void) { return 0; }

int usermode_exec(const char* path, char* const argv[], char* const envp[]) {
    (void)path;
    (void)argv;
    (void)envp;
    return -POSIX_ENOSYS;
}

void usermode_exit(int status) {
    (void)status;
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
