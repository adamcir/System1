#include "system1/fcntl.h"
#include "system1/unistd.h"
#include "../common/uutil.h"

static int show(void){
    char b[256];int fd=open("/etc/kernel.cfg",O_RDONLY),n;
    if(fd<0){u_err("kconfig: no /etc/kernel.cfg\n");return 1;}
    while((n=read(fd,b,sizeof(b)))>0)write(STDOUT_FILENO,b,(unsigned)n);
    close(fd);return 0;
}

int main(int argc,char** argv,char** envp){
    char data[192];unsigned p=0u,i;int fd; (void)envp;
    if(argc==1 || (argc==2 && u_streq(argv[1],"show"))) return show();
    if(argc!=3 || !u_streq(argv[1],"shell")){
        u_err("usage: kconfig [show | shell /bin/name.prg]\n");return 1;
    }
    {
        const char* a="shell="; const char* b="\nfallback=kernel\n";
        for(i=0;a[i];++i)data[p++]=a[i];
        for(i=0;argv[2][i] && p+1u<sizeof(data);++i)data[p++]=argv[2][i];
        for(i=0;b[i] && p+1u<sizeof(data);++i)data[p++]=b[i];
    }
    fd=open("/etc/kernel.cfg",O_CREAT|O_TRUNC|O_WRONLY);
    if(fd<0){u_err("kconfig: filesystem is read-only or unavailable\n");return 1;}
    if(write(fd,data,p)<0){close(fd);return 1;}
    close(fd);
    u_puts("kernel shell path updated; takes effect on next boot\n");
    return 0;
}
