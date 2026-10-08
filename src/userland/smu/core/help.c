#include "../common/smu.h"
#include "../common/uutil.h"

int main(int argc, char** argv, char** envp) {
    (void)argc; (void)argv; (void)envp;

    u_puts(SMU_NAME " (SMU)\n");
    u_puts("MultiShell (MSh) and independent System/1 utilities\n");
    u_puts("Builtins: cd exit\n");
    u_puts("Programs: help ls cat echo pwd mkdir touch rm ln readlink history kconfig reboot shutdown test\n");
    u_puts("Editing: Up/Down history, Left/Right, Insert, Delete, Tab completion\n");
    u_puts("Syntax: * ?  '...'  \"...\"  \\  #comment  ;  &&  ||\n");
    u_puts("Each SMU command is a separate .prg executable; SMU is not a multicall binary.\n");
    return 0;
}
