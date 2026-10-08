#include "system1/fcntl.h"
#include "system1/unistd.h"
#include "../common/uutil.h"

#define CFG_FILE "/etc/msh.cfg"
#define DEFAULT_HISTORY_FILE "/root/history"

static void history_path(char* out, unsigned cap) {
    char cfg[256];
    int fd;
    int n;
    unsigned i = 0u;

    (void)u_copy(out, cap, DEFAULT_HISTORY_FILE);

    fd = open(CFG_FILE, O_RDONLY);
    if (fd < 0) return;
    n = read(fd, cfg, sizeof(cfg) - 1u);
    (void)close(fd);
    if (n <= 0) return;
    cfg[n] = '\0';

    while (i < (unsigned)n) {
        unsigned start = i;
        unsigned end;
        unsigned key = 0u;
        const char* name = "history_file";

        while (i < (unsigned)n && cfg[i] != '\n' && cfg[i] != '\r') ++i;
        end = i;
        while (i < (unsigned)n && (cfg[i] == '\n' || cfg[i] == '\r')) ++i;

        if (start >= end || cfg[start] == '#') continue;

        while (name[key] != '\0' && start + key < end &&
               cfg[start + key] == name[key]) {
            ++key;
        }

        if (name[key] == '\0' && start + key < end && cfg[start + key] == '=') {
            unsigned p = 0u;
            unsigned j = start + key + 1u;
            while (j < end && p + 1u < cap) out[p++] = cfg[j++];
            out[p] = '\0';
            return;
        }
    }
}

static int show_history(const char* path) {
    char buf[256];
    int fd = open(path, O_RDONLY);
    int n;

    if (fd < 0) return 0;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        (void)write(STDOUT_FILENO, buf, (unsigned)n);
    }

    (void)close(fd);
    return 0;
}

static int clear_history(const char* path) {
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY);
    if (fd < 0) {
        u_err("history: cannot clear history\n");
        return 1;
    }
    (void)close(fd);
    return 0;
}

int main(int argc, char** argv, char** envp) {
    char path[96];
    (void)envp;

    history_path(path, sizeof(path));

    if (argc == 1) return show_history(path);
    if (argc == 2 && u_streq(argv[1], "-c")) return clear_history(path);

    u_err("usage: history [-c]\n");
    return 1;
}
