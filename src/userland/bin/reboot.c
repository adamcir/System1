#include "system1/reboot.h"
#include "../common/uutil.h"

int main(int argc, char** argv, char** envp) {
    (void)argc;
    (void)argv;
    (void)envp;

    u_puts("Rebooting...\n");
    if (reboot(RB_AUTOBOOT) < 0) {
        u_err("reboot: failed\n");
        return 1;
    }
    return 0;
}
