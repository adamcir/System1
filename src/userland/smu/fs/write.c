#include "system1/fcntl.h"
#include "system1/unistd.h"
#include "../common/uutil.h"

#define WRITE_BUFFER_CAP 512u

int main(int argc, char** argv, char** envp) {
    char buffer[WRITE_BUFFER_CAP];
    unsigned len = 0u;
    int append = 0;
    int newline = 1;
    int first = 1;
    int i;
    int fd;
    (void)envp;

    while (first < argc) {
        if (u_streq(argv[first], "--")) {
            ++first;
            break;
        }
        if (u_streq(argv[first], "-a")) {
            append = 1;
            ++first;
            continue;
        }
        if (u_streq(argv[first], "-n")) {
            newline = 0;
            ++first;
            continue;
        }
        break;
    }

    if (first >= argc) {
        u_err("usage: write [-a] [-n] file [text ...]\n");
        return 1;
    }

    for (i = first + 1; i < argc; ++i) {
        unsigned j = 0u;
        if (i > first + 1) {
            if (len + 1u >= sizeof(buffer)) {
                u_err("write: text too long\n");
                return 1;
            }
            buffer[len++] = ' ';
        }
        while (argv[i][j] != '\0') {
            if (len + 1u >= sizeof(buffer)) {
                u_err("write: text too long\n");
                return 1;
            }
            buffer[len++] = argv[i][j++];
        }
    }

    if (newline) {
        if (len + 1u >= sizeof(buffer)) {
            u_err("write: text too long\n");
            return 1;
        }
        buffer[len++] = '\n';
    }

    fd = open(argv[first],
              O_CREAT | O_WRONLY | (append ? O_APPEND : O_TRUNC));
    if (fd < 0) {
        u_err("write: cannot open ");
        u_err(argv[first]);
        u_err("\n");
        return 1;
    }

    if (len != 0u && write(fd, buffer, len) != (int)len) {
        (void)close(fd);
        u_err("write: write failed\n");
        return 1;
    }

    if (close(fd) < 0) {
        u_err("write: close failed\n");
        return 1;
    }
    return 0;
}
