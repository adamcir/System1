#include "system1/fcntl.h"
#include "system1/unistd.h"
#include "../common/uutil.h"
int main(int argc, char** argv, char** envp) {
    char buf[256]; int i; (void)envp;
    if (argc < 2) { u_err("cat: missing file\n"); return 1; }
    for (i=1;i<argc;++i) {
        int fd=open(argv[i],O_RDONLY), n;
        if(fd<0){u_err("cat: cannot open ");u_err(argv[i]);u_err("\n");continue;}
        while((n=read(fd,buf,sizeof(buf)))>0) (void)write(STDOUT_FILENO,buf,(unsigned)n);
        (void)close(fd);
    }
    return 0;
}
