#include "system1/ioctl.h"
#include "system1/unistd.h"
#include "../common/uutil.h"

int main(int argc, char** argv, char** envp) {
    (void)argc;
    (void)argv;
    (void)envp;

    if (ioctl(STDOUT_FILENO, TIOCCLEAR, 0u) < 0) {
        u_err("clear: stdout is not a terminal\n");
        return 1;
    }
    return 0;
}
