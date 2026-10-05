#include "system1/fcntl.h"
#include "system1/unistd.h"
#include "../common/uutil.h"

#define LINE_CAP 192u
#define ARGV_CAP 12u
#define PATH_CAP 96u
#define HISTORY_PATH_CAP 96u
#define MOTD_PATH_CAP 96u

#define PROMPT_MODE_CWD  0u
#define PROMPT_MODE_NAME 1u

static char g_path[64] = "/bin";
static char g_prompt_text[32] = "msh";
static char g_prompt_suffix[16] = " > ";
static char g_history_file[HISTORY_PATH_CAP] = "/root/history";
static char g_motd_file[MOTD_PATH_CAP] = "/etc/motd";
static unsigned g_prompt_mode = PROMPT_MODE_CWD;
static unsigned g_banner = 0u;
static unsigned g_history_max_bytes = 3072u;

static int key_matches(const char* buf, unsigned start, unsigned end, const char* key) {
    unsigned i = 0u;
    while (key[i] != '\0') {
        if (start + i >= end || buf[start + i] != key[i]) return 0;
        ++i;
    }
    return (start + i < end && buf[start + i] == '=');
}

static void copy_value(char* dst, unsigned cap,
                       const char* buf, unsigned start, unsigned end,
                       unsigned key_len) {
    unsigned p = 0u;
    unsigned i = start + key_len + 1u;

    if (cap == 0u) return;
    while (i < end && p + 1u < cap) dst[p++] = buf[i++];
    dst[p] = '\0';
}

static unsigned parse_uint(const char* s, unsigned fallback) {
    unsigned value = 0u;
    unsigned i = 0u;

    if (s == 0 || s[0] == '\0') return fallback;
    while (s[i] != '\0') {
        if (s[i] < '0' || s[i] > '9') return fallback;
        value = value * 10u + (unsigned)(s[i] - '0');
        ++i;
    }
    return value;
}

static void load_config(void) {
    char buf[320];
    char value[96];
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

        if (start >= end || buf[start] == '#') continue;

        if (key_matches(buf, start, end, "path")) {
            copy_value(g_path, sizeof(g_path), buf, start, end, 4u);
        } else if (key_matches(buf, start, end, "prompt_mode")) {
            copy_value(value, sizeof(value), buf, start, end, 11u);
            g_prompt_mode = u_streq(value, "name") ? PROMPT_MODE_NAME : PROMPT_MODE_CWD;
        } else if (key_matches(buf, start, end, "prompt_text")) {
            copy_value(g_prompt_text, sizeof(g_prompt_text), buf, start, end, 11u);
        } else if (key_matches(buf, start, end, "prompt_suffix")) {
            copy_value(g_prompt_suffix, sizeof(g_prompt_suffix), buf, start, end, 13u);
        } else if (key_matches(buf, start, end, "banner")) {
            copy_value(value, sizeof(value), buf, start, end, 6u);
            g_banner = u_streq(value, "0") ? 0u : 1u;
        } else if (key_matches(buf, start, end, "history_file")) {
            copy_value(g_history_file, sizeof(g_history_file), buf, start, end, 12u);
        } else if (key_matches(buf, start, end, "history_max_bytes")) {
            copy_value(value, sizeof(value), buf, start, end, 17u);
            g_history_max_bytes = parse_uint(value, 3072u);
        } else if (key_matches(buf, start, end, "motd")) {
            copy_value(g_motd_file, sizeof(g_motd_file), buf, start, end, 4u);
        }
    }
}

static void show_motd(void) {
    char* argv[3];

    if (g_motd_file[0] == '\0') return;

    argv[0] = "cat";
    argv[1] = g_motd_file;
    argv[2] = 0;
    (void)execve("/bin/cat.prg", argv, 0);
}

static void append_history(const char* line) {
    char record[LINE_CAP + 1u];
    unsigned len;
    unsigned i;
    int fd;
    int end;

    if (line == 0 || line[0] == '\0' || g_history_file[0] == '\0' ||
        g_history_max_bytes == 0u) {
        return;
    }

    len = u_strlen(line);
    if (len + 1u >= sizeof(record) || len + 1u > g_history_max_bytes) return;

    for (i = 0u; i < len; ++i) record[i] = line[i];
    record[len++] = '\n';

    fd = open(g_history_file, O_CREAT | O_RDWR);
    if (fd < 0) return;

    end = lseek(fd, 0, SEEK_END);
    if (end < 0 || (unsigned)end + len > g_history_max_bytes) {
        (void)close(fd);
        fd = open(g_history_file, O_CREAT | O_TRUNC | O_WRONLY);
        if (fd < 0) return;
    }

    (void)write(fd, record, len);
    (void)close(fd);
}

static void print_prompt(void) {
    char cwd[128];

    if (g_prompt_mode == PROMPT_MODE_CWD) {
        if (getcwd(cwd, sizeof(cwd)) != 0 && cwd[0] != '\0') {
            u_puts(cwd);
        } else {
            u_puts("/");
        }
    } else {
        u_puts(g_prompt_text);
    }

    u_puts(g_prompt_suffix);
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
            path[n++] = '.';
            path[n++] = 'p';
            path[n++] = 'r';
            path[n++] = 'g';
            path[n] = '\0';
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

    (void)argc;
    (void)argv;
    (void)envp;

    load_config();
    show_motd();
    if (g_banner != 0u) u_puts("MultiShell (MSh)\n");

    for (;;) {
        print_prompt();

        n = read(STDIN_FILENO, line, sizeof(line) - 1u);
        if (n < 0) {
            u_err("msh: input failed\n");
            return 1;
        }
        if (n == 0) continue;
        line[n] = '\0';

        append_history(line);

        ac = tokenize(line, args, ARGV_CAP);
        if (ac <= 0) continue;
        args[ac] = 0;

        if (u_streq(args[0], "exit")) return 0;

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
