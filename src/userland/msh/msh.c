#include "system1/fcntl.h"
#include "system1/unistd.h"
#include "../common/uutil.h"

#define LINE_CAP 192u
#define ARGV_CAP 12u
#define PATH_CAP 96u

static char g_prompt[32] = "msh> ";
static char g_path[64] = "/bin";

static void load_config(void) {
    char buf[192];
    int fd = open("/etc/msh.cfg", O_RDONLY);
    int n;
    unsigned i = 0u;

    if (fd < 0) return;
    n = read(fd, buf, sizeof(buf) - 1u);
    (void)close(fd);
    if (n <= 0) return;
    buf[n] = '\0';

    while (i < (unsigned)n) {
        unsigned start = i;
        unsigned end;
        while (i < (unsigned)n && buf[i] != '\n' && buf[i] != '\r') ++i;
        end = i;
        while (i < (unsigned)n && (buf[i] == '\n' || buf[i] == '\r')) ++i;

        if (end > start + 7u &&
            buf[start] == 'p' && buf[start+1u] == 'r' &&
            buf[start+2u] == 'o' && buf[start+3u] == 'm' &&
            buf[start+4u] == 'p' && buf[start+5u] == 't' &&
            buf[start+6u] == '=') {
            unsigned p = 0u;
            unsigned j = start + 7u;
            while (j < end && p + 1u < sizeof(g_prompt)) g_prompt[p++] = buf[j++];
            g_prompt[p] = '\0';
        } else if (end > start + 5u &&
                   buf[start] == 'p' && buf[start+1u] == 'a' &&
                   buf[start+2u] == 't' && buf[start+3u] == 'h' &&
                   buf[start+4u] == '=') {
            unsigned p = 0u;
            unsigned j = start + 5u;
            while (j < end && p + 1u < sizeof(g_path)) g_path[p++] = buf[j++];
            g_path[p] = '\0';
        }
    }
}

static int tokenize(char* line, char** argv, unsigned cap) {
    unsigned argc = 0u;
    char* p = line;

    while (*p != '\0') {
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0') break;
        if (argc >= cap) return -1;
        argv[argc++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t') ++p;
        if (*p != '\0') *p++ = '\0';
    }
    return (int)argc;
}

static int run_external(int argc, char** argv) {
    char path[PATH_CAP];

    if (u_has_slash(argv[0])) {
        if (u_copy(path, sizeof(path), argv[0]) != 0) return -1;
    } else {
        if (u_join3(path, sizeof(path), g_path, "/", argv[0]) != 0) return -1;
        if (!u_ends_prg(path)) {
            unsigned n = u_strlen(path);
            if (n + 4u >= sizeof(path)) return -1;
            path[n++]='.'; path[n++]='p'; path[n++]='r'; path[n++]='g'; path[n]='\0';
        }
    }

    (void)argc;
    return execve(path, argv, 0);
}

int main(int argc, char** argv, char** envp) {
    char line[LINE_CAP];
    char* args[ARGV_CAP + 1u];
    int n;
    int ac;
    int rc;
    (void)argc; (void)argv; (void)envp;

    load_config();
    u_puts("MultiShell (MSh)\n");

    for (;;) {
        u_puts(g_prompt);
        n = read(STDIN_FILENO, line, sizeof(line) - 1u);
        if (n < 0) return 1;
        if (n == 0) continue;
        line[n] = '\0';

        ac = tokenize(line, args, ARGV_CAP);
        if (ac <= 0) continue;
        args[ac] = 0;

        if (u_streq(args[0], "exit")) {
            return 0;
        }

        if (u_streq(args[0], "cd")) {
            const char* dir = (ac > 1) ? args[1] : "/";
            if (chdir(dir) < 0) u_err("msh: cd failed\n");
            continue;
        }

        rc = run_external(ac, args);
        if (rc < 0) {
            u_err("msh: command not found or failed: ");
            u_err(args[0]);
            u_err("\n");
        }
    }
}
