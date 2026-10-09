#include "system1/dirent.h"
#include "system1/fcntl.h"
#include "system1/ioctl.h"
#include "system1/unistd.h"
#include "../common/smu.h"
#include "../common/uutil.h"

#define LINE_CAP 192u
#define TOKEN_CAP 24u
#define TOKEN_LEN 96u
#define EXEC_ARG_CAP 32u
#define PATH_CAP 96u
#define HISTORY_CAP 16u
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

static char g_history[HISTORY_CAP][LINE_CAP];
static unsigned g_history_count;
static unsigned g_history_head;
static char g_history_file_buf[3073];

static char g_tokens[TOKEN_CAP][TOKEN_LEN];
static unsigned char g_token_glob[TOKEN_CAP];
static char g_glob_storage[EXEC_ARG_CAP][PATH_CAP];

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

static void history_mem_push(const char* line) {
    unsigned i;
    unsigned slot;

    if (line == 0 || line[0] == '\0') return;

    if (g_history_count > 0u) {
        slot = (g_history_head + HISTORY_CAP - 1u) % HISTORY_CAP;
        if (u_streq(g_history[slot], line)) return;
    }

    slot = g_history_head;
    for (i = 0u; line[i] != '\0' && i + 1u < LINE_CAP; ++i)
        g_history[slot][i] = line[i];
    g_history[slot][i] = '\0';

    g_history_head = (g_history_head + 1u) % HISTORY_CAP;
    if (g_history_count < HISTORY_CAP) ++g_history_count;
}

static const char* history_mem_get(unsigned reverse_index) {
    unsigned slot;
    if (reverse_index >= g_history_count) return 0;
    slot = (g_history_head + HISTORY_CAP - 1u - reverse_index) % HISTORY_CAP;
    return g_history[slot];
}

static void load_history(void) {
    int fd;
    int n;
    unsigned start = 0u;
    unsigned i;

    g_history_count = 0u;
    g_history_head = 0u;

    if (g_history_file[0] == '\0') return;
    fd = open(g_history_file, O_RDONLY);
    if (fd < 0) return;

    n = read(fd, g_history_file_buf, sizeof(g_history_file_buf) - 1u);
    (void)close(fd);
    if (n <= 0) return;
    g_history_file_buf[n] = '\0';

    for (i = 0u; i <= (unsigned)n; ++i) {
        if (g_history_file_buf[i] == '\n' || g_history_file_buf[i] == '\r' ||
            g_history_file_buf[i] == '\0') {
            char saved = g_history_file_buf[i];
            if (i > start) {
                g_history_file_buf[i] = '\0';
                history_mem_push(&g_history_file_buf[start]);
                g_history_file_buf[i] = saved;
            }
            start = i + 1u;
        }
    }
}

static void append_history(const char* line) {
    char record[LINE_CAP + 1u];
    unsigned len;
    unsigned i;
    int fd;
    int end;

    if (line == 0 || line[0] == '\0' || g_history_file[0] == '\0' ||
        g_history_max_bytes == 0u) return;

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
        if (getcwd(cwd, sizeof(cwd)) != 0 && cwd[0] != '\0') u_puts(cwd);
        else u_puts("/");
    } else {
        u_puts(g_prompt_text);
    }
    u_puts(g_prompt_suffix);
}

static int starts_with(const char* text, const char* prefix) {
    unsigned i = 0u;
    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) return 0;
        ++i;
    }
    return 1;
}

static int replace_range(char* line, unsigned cap, unsigned* len, unsigned* cursor,
                         unsigned start, unsigned end, const char* replacement) {
    unsigned rlen = u_strlen(replacement);
    unsigned old = end - start;
    unsigned i;

    if (*len - old + rlen + 1u > cap) return -1;

    if (rlen > old) {
        for (i = *len + 1u; i > end; --i)
            line[i + rlen - old - 1u] = line[i - 1u];
    } else if (rlen < old) {
        for (i = end; i <= *len; ++i)
            line[start + rlen + (i - end)] = line[i];
    }

    for (i = 0u; i < rlen; ++i) line[start + i] = replacement[i];
    *len = *len - old + rlen;
    *cursor = start + rlen;
    line[*len] = '\0';
    return 0;
}

static unsigned common_prefix(char* best, const char* candidate, unsigned have) {
    unsigned i = 0u;
    if (have == 0u) {
        (void)u_copy(best, PATH_CAP, candidate);
        return u_strlen(best);
    }
    while (best[i] != '\0' && candidate[i] != '\0' && best[i] == candidate[i]) ++i;
    best[i] = '\0';
    return i;
}

static void complete_command(char* line, unsigned cap, unsigned* len,
                             unsigned* cursor, unsigned start) {
    struct dirent entries[32];
    char prefix[PATH_CAP];
    char best[PATH_CAP];
    char candidate[PATH_CAP];
    const char* builtins[2] = {"cd", "exit"};
    unsigned p = 0u;
    unsigned common = 0u;
    unsigned matches = 0u;
    int n;
    int i;
    unsigned b;

    while (start + p < *cursor && p + 1u < sizeof(prefix)) {
        prefix[p] = line[start + p];
        ++p;
    }
    prefix[p] = '\0';

    for (b = 0u; b < 2u; ++b) {
        if (starts_with(builtins[b], prefix)) {
            common = common_prefix(best, builtins[b], matches);
            ++matches;
        }
    }

    n = getdents("/bin", entries, 32u);
    if (n > 0) {
        for (i = 0; i < n; ++i) {
            unsigned l;
            if (entries[i].d_type == DT_DIR) continue;
            if (u_copy(candidate, sizeof(candidate), entries[i].d_name) != 0) continue;
            l = u_strlen(candidate);
            if (l > 4u && candidate[l-4u] == '.' && candidate[l-3u] == 'p' &&
                candidate[l-2u] == 'r' && candidate[l-1u] == 'g') {
                candidate[l-4u] = '\0';
            }
            if (!starts_with(candidate, prefix)) continue;
            common = common_prefix(best, candidate, matches);
            ++matches;
        }
    }

    if (matches > 0u && common > u_strlen(prefix))
        (void)replace_range(line, cap, len, cursor, start, *cursor, best);
}

static void complete_path(char* line, unsigned cap, unsigned* len,
                          unsigned* cursor, unsigned start) {
    struct dirent entries[32];
    char token[PATH_CAP];
    char dir[PATH_CAP];
    char prefix[PATH_CAP];
    char base[PATH_CAP];
    char best[PATH_CAP];
    unsigned tlen = 0u;
    int slash = -1;
    unsigned i;
    int n;
    int m;
    unsigned matches = 0u;
    unsigned common = 0u;
    unsigned char unique_type = 0u;

    while (start + tlen < *cursor && tlen + 1u < sizeof(token)) {
        token[tlen] = line[start + tlen];
        if (token[tlen] == '/') slash = (int)tlen;
        ++tlen;
    }
    token[tlen] = '\0';

    if (slash >= 0) {
        if (slash == 0) {
            dir[0] = '/'; dir[1] = '\0';
        } else {
            for (i = 0u; i < (unsigned)slash && i + 1u < sizeof(dir); ++i)
                dir[i] = token[i];
            dir[i] = '\0';
        }
        for (i = 0u; i <= (unsigned)slash && i + 1u < sizeof(base); ++i)
            base[i] = token[i];
        base[i] = '\0';
        (void)u_copy(prefix, sizeof(prefix), token + slash + 1);
    } else {
        (void)u_copy(dir, sizeof(dir), ".");
        base[0] = '\0';
        (void)u_copy(prefix, sizeof(prefix), token);
    }

    n = getdents(dir, entries, 32u);
    if (n <= 0) return;

    for (m = 0; m < n; ++m) {
        if (entries[m].d_name[0] == '.' && prefix[0] != '.') continue;
        if (!starts_with(entries[m].d_name, prefix)) continue;
        common = common_prefix(best, entries[m].d_name, matches);
        unique_type = entries[m].d_type;
        ++matches;
    }

    if (matches > 0u && (common > u_strlen(prefix) ||
                         (matches == 1u && unique_type == DT_DIR))) {
        char replacement[PATH_CAP];
        if (u_join3(replacement, sizeof(replacement), base, best, "") == 0) {
            unsigned rl = u_strlen(replacement);
            if (matches == 1u && unique_type == DT_DIR &&
                rl + 1u < sizeof(replacement) && replacement[rl - 1u] != '/') {
                replacement[rl] = '/';
                replacement[rl + 1u] = '\0';
            }
            (void)replace_range(line, cap, len, cursor, start, *cursor, replacement);
        }
    }
}

static void complete_line(char* line, unsigned cap, unsigned* len, unsigned* cursor) {
    unsigned start = *cursor;
    unsigned segment = 0u;
    unsigned i;
    int command = 1;

    while (start > 0u) {
        char c = line[start - 1u];
        if (c == ' ' || c == '\t' || c == ';' || c == '&' || c == '|') break;
        --start;
    }

    for (i = 0u; i < start; ++i) {
        if (line[i] == ';' || line[i] == '|' || line[i] == '&') {
            segment = i + 1u;
            continue;
        }
    }
    for (i = segment; i < start; ++i) {
        if (line[i] != ' ' && line[i] != '\t') {
            command = 0;
            break;
        }
    }

    if (command) complete_command(line, cap, len, cursor, start);
    else complete_path(line, cap, len, cursor, start);
}

static int redraw_line(char* line, unsigned len, unsigned cursor) {
    struct system1_tty_line state;
    state.buffer = line;
    state.len = len;
    state.cursor = cursor;
    return ioctl(STDIN_FILENO, TIOCLINEREDRAW, (unsigned)(uintptr_t)&state);
}

static void set_line(char* dst, unsigned cap, unsigned* len, unsigned* cursor,
                     const char* src) {
    unsigned i = 0u;
    while (src != 0 && src[i] != '\0' && i + 1u < cap) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
    *len = i;
    *cursor = i;
}

static int read_line(char* line, unsigned cap) {
    unsigned len = 0u;
    unsigned cursor = 0u;
    unsigned char insert = 1u;
    int history_nav = -1;
    char draft[LINE_CAP];

    line[0] = '\0';
    draft[0] = '\0';
    (void)ioctl(STDIN_FILENO, TIOCLINEBEGIN, 0u);

    for (;;) {
        int key = ioctl(STDIN_FILENO, TIOCGETKEY, 0u);

        if (key < 0) return -1;

        if (key == '\n') {
            (void)redraw_line(line, len, len);
            u_puts("\n");
            line[len] = '\0';
            return (int)len;
        }

        if (key == 0x03) {
            line[0] = '\0';
            (void)redraw_line(line, 0u, 0u);
            u_puts("^C\n");
            return 0;
        }

        if (key == '\b') {
            unsigned i;
            if (cursor == 0u) continue;
            for (i = cursor; i < len; ++i) line[i - 1u] = line[i];
            --cursor; --len; line[len] = '\0';
            history_nav = -1;
            (void)redraw_line(line, len, cursor);
            continue;
        }

        if (key == (int)TTY_KEY_DELETE) {
            unsigned i;
            if (cursor >= len) continue;
            for (i = cursor + 1u; i < len; ++i) line[i - 1u] = line[i];
            --len; line[len] = '\0';
            history_nav = -1;
            (void)redraw_line(line, len, cursor);
            continue;
        }

        if (key == (int)TTY_KEY_LEFT) {
            if (cursor > 0u) --cursor;
            (void)redraw_line(line, len, cursor);
            continue;
        }

        if (key == (int)TTY_KEY_RIGHT) {
            if (cursor < len) ++cursor;
            (void)redraw_line(line, len, cursor);
            continue;
        }

        if (key == (int)TTY_KEY_INSERT) {
            insert = insert ? 0u : 1u;
            continue;
        }

        if (key == (int)TTY_KEY_UP) {
            const char* old;
            if (g_history_count == 0u) continue;
            if (history_nav < 0) (void)u_copy(draft, sizeof(draft), line);
            if ((unsigned)(history_nav + 1) < g_history_count) ++history_nav;
            old = history_mem_get((unsigned)history_nav);
            set_line(line, cap, &len, &cursor, old);
            (void)redraw_line(line, len, cursor);
            continue;
        }

        if (key == (int)TTY_KEY_DOWN) {
            if (history_nav < 0) continue;
            --history_nav;
            if (history_nav < 0) set_line(line, cap, &len, &cursor, draft);
            else set_line(line, cap, &len, &cursor, history_mem_get((unsigned)history_nav));
            (void)redraw_line(line, len, cursor);
            continue;
        }

        if (key == '\t') {
            complete_line(line, cap, &len, &cursor);
            (void)redraw_line(line, len, cursor);
            continue;
        }

        if (key > 0 && key < 128) {
            unsigned i;
            if (len + 1u >= cap) continue;
            if (!insert && cursor < len) {
                line[cursor++] = (char)key;
            } else {
                for (i = len; i > cursor; --i) line[i] = line[i - 1u];
                line[cursor++] = (char)key;
                ++len;
            }
            line[len] = '\0';
            history_nav = -1;
            (void)redraw_line(line, len, cursor);
        }
    }
}

static int parse_line(const char* line) {
    unsigned count = 0u;
    unsigned i = 0u;

    while (line[i] != '\0') {
        unsigned out = 0u;
        unsigned char quote = 0u;
        unsigned char glob = 0u;

        while (line[i] == ' ' || line[i] == '\t') ++i;
        if (line[i] == '\0' || line[i] == '#') break;
        if (count >= TOKEN_CAP) return -1;

        if (line[i] == ';') {
            g_tokens[count][0] = ';'; g_tokens[count][1] = '\0';
            g_token_glob[count++] = 0u; ++i; continue;
        }
        if (line[i] == '&' && line[i+1u] == '&') {
            g_tokens[count][0] = '&'; g_tokens[count][1] = '&'; g_tokens[count][2] = '\0';
            g_token_glob[count++] = 0u; i += 2u; continue;
        }
        if (line[i] == '|' && line[i+1u] == '|') {
            g_tokens[count][0] = '|'; g_tokens[count][1] = '|'; g_tokens[count][2] = '\0';
            g_token_glob[count++] = 0u; i += 2u; continue;
        }
        if (line[i] == '&' || line[i] == '|') return -2;

        while (line[i] != '\0') {
            char c = line[i];
            unsigned char escaped = 0u;

            if (quote == 0u && (c == ' ' || c == '\t' || c == ';' || c == '&' || c == '|'))
                break;

            if (quote == 0u && (c == '\'' || c == '"')) {
                quote = (unsigned char)c; ++i; continue;
            }
            if (quote != 0u && c == (char)quote) {
                quote = 0u; ++i; continue;
            }
            if (c == '\\' && quote != '\'') {
                ++i;
                if (line[i] == '\0') return -2;
                c = line[i++];
                escaped = 1u;
            } else {
                ++i;
            }

            if (out + 1u >= TOKEN_LEN) return -1;
            if (escaped == 0u && quote == 0u && (c == '*' || c == '?')) glob = 1u;
            g_tokens[count][out++] = c;
        }

        if (quote != 0u) return -2;
        g_tokens[count][out] = '\0';
        g_token_glob[count] = glob;
        ++count;
    }

    return (int)count;
}

static int wildcard_match(const char* pattern, const char* text) {
    if (*pattern == '\0') return *text == '\0';
    if (*pattern == '*') {
        do {
            if (wildcard_match(pattern + 1, text)) return 1;
        } while (*text++ != '\0');
        return 0;
    }
    if (*pattern == '?') return *text != '\0' && wildcard_match(pattern + 1, text + 1);
    return *pattern == *text && wildcard_match(pattern + 1, text + 1);
}

static int expand_one(const char* token, char** outv, unsigned* outc) {
    struct dirent entries[32];
    char dir[PATH_CAP];
    char base[PATH_CAP];
    const char* pattern = token;
    int slash = -1;
    unsigned i;
    int n;
    int m;
    unsigned matches = 0u;

    for (i = 0u; token[i] != '\0'; ++i) if (token[i] == '/') slash = (int)i;

    if (slash >= 0) {
        if (slash == 0) { dir[0] = '/'; dir[1] = '\0'; }
        else {
            for (i = 0u; i < (unsigned)slash && i + 1u < sizeof(dir); ++i) dir[i] = token[i];
            dir[i] = '\0';
        }
        for (i = 0u; i <= (unsigned)slash && i + 1u < sizeof(base); ++i) base[i] = token[i];
        base[i] = '\0';
        pattern = token + slash + 1;
    } else {
        (void)u_copy(dir, sizeof(dir), ".");
        base[0] = '\0';
    }

    n = getdents(dir, entries, 32u);
    if (n > 0) {
        for (m = 0; m < n && *outc < EXEC_ARG_CAP - 1u; ++m) {
            if (entries[m].d_name[0] == '.' && pattern[0] != '.') continue;
            if (!wildcard_match(pattern, entries[m].d_name)) continue;
            if (u_join3(g_glob_storage[*outc], PATH_CAP, base, entries[m].d_name, "") != 0) continue;
            outv[*outc] = g_glob_storage[*outc];
            ++(*outc);
            ++matches;
        }
    }

    if (matches == 0u && *outc < EXEC_ARG_CAP - 1u) outv[(*outc)++] = (char*)token;
    return 0;
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
            path[n++] = '.'; path[n++] = 'p'; path[n++] = 'r'; path[n++] = 'g'; path[n] = '\0';
        }
    }

    (void)argc;
    return execve(path, argv, 0);
}

static int run_segment(unsigned start, unsigned end, int* want_exit) {
    char* argv[EXEC_ARG_CAP];
    unsigned argc = 0u;
    unsigned i;
    int rc;

    if (start >= end) return -1;
    argv[argc++] = g_tokens[start];

    for (i = start + 1u; i < end && argc < EXEC_ARG_CAP - 1u; ++i) {
        if (g_token_glob[i]) (void)expand_one(g_tokens[i], argv, &argc);
        else argv[argc++] = g_tokens[i];
    }
    argv[argc] = 0;

    if (u_streq(argv[0], "exit")) {
        *want_exit = 1;
        return 0;
    }

    if (u_streq(argv[0], "cd")) {
        const char* dir = (argc > 1u) ? argv[1] : "/";
        if (argc > 2u) {
            u_err("msh: cd: too many arguments\n");
            return 1;
        }
        if (chdir(dir) < 0) {
            u_err("msh: cd failed\n");
            return 1;
        }
        return 0;
    }

    rc = run_external((int)argc, argv);
    if (rc < 0) {
        u_err("msh: command not found or failed: ");
        u_err(argv[0]);
        u_err("\n");
        return 127;
    }
    return rc;
}

static int execute_tokens(int count, int* want_exit) {
    unsigned start = 0u;
    int last = 0;
    int should_run = 1;

    while (start < (unsigned)count) {
        unsigned end = start;
        const char* op = 0;

        while (end < (unsigned)count &&
               !u_streq(g_tokens[end], ";") &&
               !u_streq(g_tokens[end], "&&") &&
               !u_streq(g_tokens[end], "||")) ++end;

        if (end == start) {
            u_err("msh: syntax error near operator\n");
            return 2;
        }

        if (should_run) {
            last = run_segment(start, end, want_exit);
            if (*want_exit) return last;
        }

        if (end >= (unsigned)count) break;
        op = g_tokens[end];
        if (end + 1u >= (unsigned)count) {
            u_err("msh: trailing operator\n");
            return 2;
        }

        if (u_streq(op, "&&")) should_run = (last == 0);
        else if (u_streq(op, "||")) should_run = (last != 0);
        else should_run = 1;

        start = end + 1u;
    }

    return last;
}

int main(int argc, char** argv, char** envp) {
    char line[LINE_CAP];
    int n;
    int count;
    int want_exit;
    (void)argc; (void)argv; (void)envp;

    load_config();
    load_history();
    show_motd();
    if (g_banner != 0u) u_puts(SMU_NAME " - MultiShell (MSh)\n");

    for (;;) {
        print_prompt();
        n = read_line(line, sizeof(line));
        if (n < 0) {
            u_err("msh: input failed\n");
            return 1;
        }
        if (n == 0) continue;

        append_history(line);
        history_mem_push(line);

        count = parse_line(line);
        if (count == -2) {
            u_err("msh: syntax error\n");
            continue;
        }
        if (count <= 0) continue;

        want_exit = 0;
        (void)execute_tokens(count, &want_exit);
        if (want_exit) return 0;
    }
}
