#include "system1/unistd.h"
#include "../common/uutil.h"
int main(int argc, char** argv, char** envp) {
    (void)envp;
    if(argc!=4 || !u_streq(argv[1],"-s")) {
        u_err("usage: ln -s target link\n"); return 1;
    }
    if(symlink(argv[2],argv[3])<0){u_err("ln: failed\n");return 1;}
    return 0;
}
