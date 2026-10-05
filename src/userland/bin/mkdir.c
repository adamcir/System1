#include "system1/unistd.h"
#include "../common/uutil.h"
int main(int argc, char** argv, char** envp) {
    int i, rc=0; (void)envp;
    if(argc<2){u_err("mkdir: missing operand\n");return 1;}
    for(i=1;i<argc;++i) if(mkdir(argv[i])<0){u_err("mkdir: failed\n");rc=1;}
    return rc;
}
