#include "system1/fcntl.h"
#include "system1/unistd.h"
#include "../common/uutil.h"

#define CFG_CAP 320u
#define SHELL_CAP 8u
#define SHELL_PATH_CAP 96u

static int read_file(const char* path, char* buf, unsigned cap) {
    int fd;
    int n;

    if (cap < 2u) return -1;
    fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    n = read(fd, buf, cap - 1u);
    (void)close(fd);
    if (n < 0) return -1;

    buf[n] = '\0';
    return n;
}

static int show_file(const char* path) {
    char buf[CFG_CAP];
    int n = read_file(path, buf, sizeof(buf));

    if (n < 0) {
        u_err("kconfig: cannot read ");
        u_err(path);
        u_err("\n");
        return 1;
    }

    u_puts(buf);
    if (n > 0 && buf[n - 1] != '\n') u_puts("\n");
    return 0;
}

static int shell_exists(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    (void)close(fd);
    return 1;
}

static int write_kernel_shell(const char* path) {
    char data[CFG_CAP];
    unsigned p = 0u;
    unsigned i;
    int fd;
    const char* a =
        "# System/1 kernel configuration\n"
        "version=1\n"
        "shell=";
    const char* b =
        "\nshells=/etc/shells\n"
        "fallback=kernel\n";

    if (!shell_exists(path)) {
        u_err("kconfig: selected shell does not exist\n");
        return 1;
    }

    for (i = 0u; a[i] != '\0' && p + 1u < sizeof(data); ++i) data[p++] = a[i];
    for (i = 0u; path[i] != '\0' && p + 1u < sizeof(data); ++i) data[p++] = path[i];
    for (i = 0u; b[i] != '\0' && p + 1u < sizeof(data); ++i) data[p++] = b[i];

    fd = open("/etc/kernel.cfg", O_CREAT | O_TRUNC | O_WRONLY);
    if (fd < 0) {
        u_err("kconfig: filesystem is read-only or unavailable\n");
        return 1;
    }
    if (write(fd, data, p) < 0) {
        (void)close(fd);
        u_err("kconfig: write failed\n");
        return 1;
    }
    (void)close(fd);

    u_puts("Selected shell: ");
    u_puts(path);
    u_puts("\nTakes effect on next boot.\n");
    return 0;
}

static int parse_shells(char* buf, char** shells, unsigned cap) {
    unsigned count = 0u;
    char* p = buf;

    while (*p != '\0') {
        char* start;

        while (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t') ++p;
        if (*p == '\0') break;

        if (*p == '#') {
            while (*p != '\0' && *p != '\n' && *p != '\r') ++p;
            continue;
        }

        start = p;
        while (*p != '\0' && *p != '\n' && *p != '\r') ++p;
        if (*p != '\0') *p++ = '\0';

        if (count < cap && start[0] == '/') shells[count++] = start;
    }

    return (int)count;
}

static int select_shell(void) {
    char buf[CFG_CAP];
    char answer[16];
    char* shells[SHELL_CAP];
    int count;
    int n;
    int index;
    int i;
    char number[4];

    if (read_file("/etc/shells", buf, sizeof(buf)) < 0) {
        u_err("kconfig: cannot read /etc/shells\n");
        return 1;
    }

    count = parse_shells(buf, shells, SHELL_CAP);
    if (count <= 0) {
        u_err("kconfig: /etc/shells contains no shells\n");
        return 1;
    }

    u_puts("Available System/1 shells:\n");
    for (i = 0; i < count; ++i) {
        number[0] = (char)('1' + i);
        number[1] = ')';
        number[2] = ' ';
        number[3] = '\0';
        u_puts(number);
        u_puts(shells[i]);
        u_puts("\n");
    }

    u_puts("Select shell [1-");
    number[0] = (char)('0' + count);
    number[1] = ']';
    number[2] = ':';
    number[3] = '\0';
    u_puts(number);
    u_puts(" ");

    n = read(STDIN_FILENO, answer, sizeof(answer) - 1u);
    if (n <= 0) return 1;
    answer[n] = '\0';

    index = (int)(answer[0] - '1');
    if (index < 0 || index >= count) {
        u_err("kconfig: invalid selection\n");
        return 1;
    }

    return write_kernel_shell(shells[index]);
}

static int interactive_menu(void) {
    char line[16];
    int n;

    for (;;) {
        u_puts("\nSystem/1 Kernel Configurator\n");
        u_puts("1) Show kernel configuration\n");
        u_puts("2) Select userspace shell\n");
        u_puts("3) Show valid shells\n");
        u_puts("4) Show MultiShell configuration\n");
        u_puts("q) Exit\n");
        u_puts("Selection: ");

        n = read(STDIN_FILENO, line, sizeof(line) - 1u);
        if (n <= 0) continue;
        line[n] = '\0';

        if (line[0] == '1') {
            (void)show_file("/etc/kernel.cfg");
        } else if (line[0] == '2') {
            (void)select_shell();
        } else if (line[0] == '3') {
            (void)show_file("/etc/shells");
        } else if (line[0] == '4') {
            (void)show_file("/etc/msh.cfg");
        } else if (line[0] == 'q' || line[0] == 'Q') {
            return 0;
        } else {
            u_err("Unknown selection.\n");
        }
    }
}

int main(int argc, char** argv, char** envp) {
    (void)envp;

    if (argc == 1) return interactive_menu();

    if (argc == 2 && u_streq(argv[1], "show"))
        return show_file("/etc/kernel.cfg");

    if (argc == 2 && u_streq(argv[1], "shell"))
        return select_shell();

    if (argc == 3 && u_streq(argv[1], "shell"))
        return write_kernel_shell(argv[2]);

    if (argc == 2 && u_streq(argv[1], "shells"))
        return show_file("/etc/shells");

    if (argc == 2 && u_streq(argv[1], "msh"))
        return show_file("/etc/msh.cfg");

    u_err("usage: kconfig [show | shell [path] | shells | msh]\n");
    return 1;
}
