#include "system1/dirent.h"
#include "system1/unistd.h"
#include "../common/uutil.h"
int main(int argc, char** argv, char** envp) {
    struct dirent e[32]; const char* path=(argc>1)?argv[1]:"."; int n,i; (void)envp;
    n=getdents(path,e,32u);
    if(n<0){u_err("ls: failed\n");return 1;}
    for(i=0;i<n;++i){
        u_puts(e[i].d_name);
        if(e[i].d_type==DT_DIR)u_puts("/");
        else if(e[i].d_type==DT_LNK)u_puts("@");
        u_puts("\n");
    }
    return 0;
}
