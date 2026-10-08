#include "system1/unistd.h"
#include "../common/uutil.h"

int main(int argc, char** argv, char** envp) {
    (void)argc;
    (void)argv;
    (void)envp;

    sync();
    return 0;
}
