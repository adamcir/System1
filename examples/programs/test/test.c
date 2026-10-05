#include "system1/unistd.h"

static const char message[] =
    "Hello from /bin/test.prg - System/1 userspace!\n";

void _start(void) {
    int rc = write(STDOUT_FILENO, message, sizeof(message) - 1u);
    _exit((rc < 0) ? 1 : 0);
}
