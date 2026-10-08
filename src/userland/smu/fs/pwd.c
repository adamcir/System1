#include "system1/unistd.h"
#include "../common/uutil.h"
int main(int argc, char** argv, char** envp) {
    char cwd[128]; (void)argc; (void)argv; (void)envp;
    if (getcwd(cwd,sizeof(cwd))==0) return 1;
    u_puts(cwd); u_puts("\n"); return 0;
}
