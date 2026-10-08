#include "system1/unistd.h"
#include "../common/uutil.h"

static int parse_uint(const char* s, unsigned* out) {
    unsigned value = 0u;
    unsigned i = 0u;

    if (s == 0 || s[0] == '\0') return -1;
    while (s[i] != '\0') {
        if (s[i] < '0' || s[i] > '9') return -1;
        value = value * 10u + (unsigned)(s[i] - '0');
        ++i;
    }
    *out = value;
    return 0;
}

int main(int argc, char** argv, char** envp) {
    unsigned seconds;
    (void)envp;

    if (argc != 2 || parse_uint(argv[1], &seconds) != 0) {
        u_err("usage: sleep seconds\n");
        return 1;
    }

    (void)sleep(seconds);
    return 0;
}
