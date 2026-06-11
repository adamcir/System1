#include "system1/unistd.h"

static const char message[] = "System/1 SPRG test program\n";

void _start(void) {
    (void)write(1, message, sizeof(message) - 1u);

    for (;;) {
    }
}
