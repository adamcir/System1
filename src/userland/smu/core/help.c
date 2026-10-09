#include "../common/smu.h"
#include "../common/uutil.h"

int main(int argc, char** argv, char** envp) {
    (void)argc; (void)argv; (void)envp;

    u_puts(SMU_NAME " (SMU)\n");
    u_puts("MultiShell (MSh) and independent System/1 utilities\n");
    u_puts("Builtins: cd exit\n");
    u_puts("Programs: help clear echo ls cat pwd mkdir touch write rm ln readlink stat history sleep true false adatext kconfig sync reboot shutdown\n");
    u_puts("Editing: Up/Down history, Left/Right, Insert, Delete, Tab completion\n");
    u_puts("Directories: TAB adds '/', ls -la, rm -rf dir/\n");
    u_puts("Devices: /dev/tty, /dev/console, /dev/ttyS0, /dev/kmsg\n");
    u_puts("Syntax: * ?  '...'  \"...\"  \\  #comment  ;  &&  ||\n");
    u_puts("Examples: write file.txt text | write -a file.txt more | stat file.txt | sleep 1\n");
    u_puts("Each SMU command is a separate .prg executable; SMU is not a multicall binary.\n");
    return 0;
}
