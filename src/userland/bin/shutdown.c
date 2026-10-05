#include "system1/reboot.h"
#include "../common/uutil.h"

int main(int argc, char** argv, char** envp) {
    (void)argc;
    (void)argv;
    (void)envp;

    u_puts("Shutting down...\n");
    if (reboot(RB_POWER_OFF) < 0) {
        u_err("shutdown: failed\n");
        return 1;
    }
    return 0;
}
