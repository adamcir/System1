#include "../common/uutil.h"

int main(int argc, char** argv, char** envp) {
    (void)argc; (void)argv; (void)envp;

    u_puts("MultiShell (MSh)\n");
    u_puts("Builtins: cd exit\n");
    u_puts("Programs: help ls cat echo pwd mkdir touch rm ln readlink history kconfig reboot shutdown test\n");
    u_puts("Editing: Up/Down history, Left/Right, Insert, Delete, Tab completion\n");
    u_puts("Syntax: * ?  '...'  \"...\"  \\  #comment  ;  &&  ||\n");
    u_puts("Examples:\n");
    u_puts("  ls *.prg\n");
    u_puts("  echo *.txt\n");
    u_puts("  cat /root/*.txt\n");
    u_puts("  test && echo success\n");
    u_puts("  test || echo failed\n");
    return 0;
}
