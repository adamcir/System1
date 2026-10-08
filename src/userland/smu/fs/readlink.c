#include "system1/unistd.h"
#include "../common/uutil.h"
int main(int argc, char** argv, char** envp) {
    char buf[128]; int n; (void)envp;
    if(argc!=2){u_err("usage: readlink path\n");return 1;}
    n=readlink(argv[1],buf,sizeof(buf)-1u);
    if(n<0){u_err("readlink: not a symlink\n");return 1;}
    buf[n]='\0';u_puts(buf);u_puts("\n");return 0;
}
