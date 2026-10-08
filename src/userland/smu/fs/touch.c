#include "system1/fcntl.h"
#include "system1/unistd.h"
#include "../common/uutil.h"
int main(int argc, char** argv, char** envp) {
    int i, rc=0; (void)envp;
    if(argc<2){u_err("touch: missing file\n");return 1;}
    for(i=1;i<argc;++i){int fd=open(argv[i],O_CREAT|O_RDWR);if(fd<0)rc=1;else close(fd);}
    return rc;
}
