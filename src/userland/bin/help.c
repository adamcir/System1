#include "../common/uutil.h"
int main(int argc, char** argv, char** envp) {
    (void)argc;(void)argv;(void)envp;
    u_puts("MSh commands are programs in /bin.\n");
    u_puts("Builtins: cd exit\n");
    u_puts("Programs: help ls cat echo pwd mkdir touch rm ln readlink history kconfig test\n");
    return 0;
}
