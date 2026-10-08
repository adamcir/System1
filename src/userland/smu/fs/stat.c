#include "system1/stat.h"
#include "system1/unistd.h"
#include "../common/uutil.h"

static void put_uint(unsigned value) {
    char digits[10];
    unsigned count = 0u;

    if (value == 0u) {
        u_puts("0");
        return;
    }

    while (value != 0u && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (count > 0u) {
        char c = digits[--count];
        (void)write(STDOUT_FILENO, &c, 1u);
    }
}

static const char* type_name(unsigned mode) {
    if (S_ISDIR(mode)) return "directory";
    if (S_ISREG(mode)) return "file";
    if (S_ISLNK(mode)) return "symlink";
    if (S_ISCHR(mode)) return "character device";
    return "unknown";
}

int main(int argc, char** argv, char** envp) {
    int i;
    int rc = 0;
    (void)envp;

    if (argc < 2) {
        u_err("stat: missing operand\n");
        return 1;
    }

    for (i = 1; i < argc; ++i) {
        struct stat st;

        if (stat(argv[i], &st) < 0) {
            u_err("stat: cannot stat ");
            u_err(argv[i]);
            u_err("\n");
            rc = 1;
            continue;
        }

        u_puts("  File: ");
        u_puts(argv[i]);
        u_puts("\n  Type: ");
        u_puts(type_name(st.st_mode));
        u_puts("\n  Size: ");
        put_uint(st.st_size);
        u_puts(" bytes\n");
    }

    return rc;
}
