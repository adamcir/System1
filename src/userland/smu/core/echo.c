#include "system1/unistd.h"
#include "../common/uutil.h"
int main(int argc, char** argv, char** envp) {
    int i; (void)envp;
    for (i=1;i<argc;++i) {
        if (i>1) u_puts(" ");
        u_puts(argv[i]);
    }
    u_puts("\n");
    return 0;
}
