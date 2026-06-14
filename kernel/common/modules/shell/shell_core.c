#include "shell_core.h"
#include "types.h"
#include "interrupts.h"
#include "signals.h"
#include "tty.h"
#include "klog.h"
#include "fs.h"
#include "fs_core.h"
#include "mm.h"
#include "posix.h"

#define SHELL_LINE_CAP 256u
#define SHELL_ARGV_MAX 16u
#define SHELL_HISTORY_CAP 16u
#define SHELL_LIST_CAP 16u
#define SHELL_CAT_BUF_CAP 4096u

static const char* g_shell_commands[] = {
    "help", "clear", "echo", "reboot", "shutdown", "ticks",
    "version", "pwd", "ls", "cd", "mkdir", "cat", "stat",
    "touch", "write", "rm", "exec", "mmstat"
};

static char g_shell_history[SHELL_HISTORY_CAP][SHELL_LINE_CAP];
static uint32_t g_shell_history_count;
static uint32_t g_shell_history_head;
static int g_shell_history_nav = -1;
static char g_shell_history_draft[SHELL_LINE_CAP];
static char g_shell_prev_dir[FS_PATH_CAP] = "/";

static void shell_print_fs_error(const char* cmd, int rc);
static void shell_print_posix_path_error(const char* cmd, const char* path, int rc);
static void shell_print_usage(const char* cmd, const char* usage);

static int shell_streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }
    return (int)(*a == '\0' && *b == '\0');
}

static int shell_is_option(const char* arg) {
    return (int)(arg != 0 && arg[0] == '-' && arg[1] != '\0');
}

static int shell_is_directory_mode(uint32_t mode) {
    return (int)((mode & FS_MODE_DIR) == FS_MODE_DIR);
}

static int shell_is_space(char c) {
    return (int)(c == ' ' || c == '\t');
}

static int shell_tokenize(char* line, char** argv, uint32_t argv_cap, uint32_t* out_argc) {
    uint32_t argc = 0u;
    char* read = line;
    char* write = line;
    char* token_end;

    if (out_argc == 0) {
        return -1;
    }

    *out_argc = 0u;

    while (*read != '\0') {
        uint8_t in_single = 0u;
        uint8_t in_double = 0u;
        uint8_t token_started = 0u;

        while (shell_is_space(*read) != 0) {
            ++read;
        }

        if (*read == '\0') {
            break;
        }

        if (argc >= argv_cap) {
            return -2;
        }

        argv[argc++] = write;

        while (*read != '\0') {
            if (in_single == 0u && in_double == 0u && shell_is_space(*read) != 0) {
                break;
            }

            if (in_double == 0u && *read == '\'') {
                in_single = (uint8_t)(in_single ? 0u : 1u);
                token_started = 1u;
                ++read;
                continue;
            }

            if (in_single == 0u && *read == '"') {
                in_double = (uint8_t)(in_double ? 0u : 1u);
                token_started = 1u;
                ++read;
                continue;
            }

            if (in_single == 0u && *read == '\\') {
                char escaped = read[1];

                if (escaped == '\0') {
                    return -1;
                }

                if (escaped == '\n') {
                    read += 2;
                    continue;
                }

                if (in_double != 0u &&
                    escaped != '"' &&
                    escaped != '\\' &&
                    escaped != '$' &&
                    escaped != '`') {
                    *write++ = *read++;
                    token_started = 1u;
                    continue;
                }

                ++read;
                *write++ = *read++;
                token_started = 1u;
                continue;
            }

            *write++ = *read++;
            token_started = 1u;
        }

        if (in_single != 0u || in_double != 0u || token_started == 0u) {
            return -1;
        }

        token_end = read;

        while (shell_is_space(*read) != 0) {
            ++read;
        }

        *write++ = '\0';

        if (read > token_end && read < write) {
            read = write;
        }
    }

    *out_argc = argc;
    return 0;
}

static uint32_t shell_strlen(const char* s) {
    uint32_t len = 0u;

    while (s[len] != '\0') {
        ++len;
    }

    return len;
}

static int shell_startswith(const char* text, const char* prefix) {
    uint32_t i = 0u;

    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return 0;
        }
        ++i;
    }

    return 1;
}

static void shell_memcpy(char* dst, const char* src, uint32_t len) {
    uint32_t i;

    for (i = 0u; i < len; ++i) {
        dst[i] = src[i];
    }
}

static void shell_copy_string(char* dst, uint32_t cap, const char* src) {
    uint32_t i = 0u;

    if (cap == 0u) {
        return;
    }

    while (src[i] != '\0' && (i + 1u) < cap) {
        dst[i] = src[i];
        ++i;
    }

    dst[i] = '\0';
}

static void shell_history_reset_line_state(void) {
    g_shell_history_nav = -1;
    g_shell_history_draft[0] = '\0';
}

static uint32_t shell_history_slot(uint32_t reverse_index) {
    return (uint32_t)((g_shell_history_head + SHELL_HISTORY_CAP - 1u - reverse_index) % SHELL_HISTORY_CAP);
}

static void shell_history_push(const char* line) {
    uint32_t len;

    if (line == 0 || line[0] == '\0') {
        return;
    }

    if (g_shell_history_count > 0u) {
        const char* last = g_shell_history[shell_history_slot(0u)];
        if (shell_streq(last, line) != 0) {
            return;
        }
    }

    len = shell_strlen(line);
    if (len >= SHELL_LINE_CAP) {
        len = SHELL_LINE_CAP - 1u;
    }

    shell_memcpy(g_shell_history[g_shell_history_head], line, len);
    g_shell_history[g_shell_history_head][len] = '\0';
    g_shell_history_head = (uint32_t)((g_shell_history_head + 1u) % SHELL_HISTORY_CAP);

    if (g_shell_history_count < SHELL_HISTORY_CAP) {
        ++g_shell_history_count;
    }
}

static void shell_replace_line(char* buf, uint32_t cap, uint32_t* len, uint32_t* cursor, const char* text) {
    uint32_t new_len = shell_strlen(text);

    if (new_len >= cap) {
        new_len = cap - 1u;
    }

    shell_memcpy(buf, text, new_len);
    buf[new_len] = '\0';
    *len = new_len;
    *cursor = new_len;
}

static int shell_history_hook(int direction, char* buf, uint32_t cap, uint32_t* out_len, uint32_t* out_cursor, void* ctx) {
    uint32_t next_nav;
    (void)ctx;

    if (g_shell_history_count == 0u) {
        return 0;
    }

    if (direction < 0) {
        if (g_shell_history_nav < 0) {
            shell_copy_string(g_shell_history_draft, SHELL_LINE_CAP, buf);
            g_shell_history_nav = 0;
        } else {
            next_nav = (uint32_t)(g_shell_history_nav + 1);
            if (next_nav >= g_shell_history_count) {
                return 0;
            }
            g_shell_history_nav = (int)next_nav;
        }

        shell_replace_line(buf, cap, out_len, out_cursor, g_shell_history[shell_history_slot((uint32_t)g_shell_history_nav)]);
        return 1;
    }

    if (g_shell_history_nav < 0) {
        return 0;
    }

    if (g_shell_history_nav == 0) {
        g_shell_history_nav = -1;
        shell_replace_line(buf, cap, out_len, out_cursor, g_shell_history_draft);
        return 1;
    }

    g_shell_history_nav--;
    shell_replace_line(buf, cap, out_len, out_cursor, g_shell_history[shell_history_slot((uint32_t)g_shell_history_nav)]);
    return 1;
}

static int shell_replace_token(char* buf, uint32_t cap, uint32_t* len, uint32_t* cursor, uint32_t token_start, uint32_t token_end, const char* replacement) {
    uint32_t replacement_len = shell_strlen(replacement);
    uint32_t suffix_len = *len - token_end;
    uint32_t new_len;
    uint32_t i;

    new_len = token_start + replacement_len + suffix_len;
    if (new_len >= cap) {
        return 0;
    }

    for (i = 0u; i < suffix_len; ++i) {
        buf[token_start + replacement_len + i] = buf[token_end + i];
    }

    shell_memcpy(buf + token_start, replacement, replacement_len);
    *len = new_len;
    buf[new_len] = '\0';
    *cursor = token_start + replacement_len;
    return 1;
}

static int shell_find_unique_command(const char* prefix, const char** out_match) {
    uint32_t i;
    const char* match = 0;

    for (i = 0u; i < (uint32_t)(sizeof(g_shell_commands) / sizeof(g_shell_commands[0])); ++i) {
        if (shell_startswith(g_shell_commands[i], prefix) == 0) {
            continue;
        }

        if (match != 0) {
            return 0;
        }

        match = g_shell_commands[i];
    }

    if (match == 0) {
        return 0;
    }

    *out_match = match;
    return 1;
}

static int shell_find_unique_path_match(const char* token, char* completed, uint32_t completed_cap) {
    fs_dirent_t entries[SHELL_LIST_CAP];
    char dir_path[SHELL_LINE_CAP];
    char prefix[SHELL_LINE_CAP];
    const char* match_name = 0;
    uint8_t match_type = 0u;
    const char* slash = 0;
    uint32_t count = 0u;
    uint32_t token_len = shell_strlen(token);
    uint32_t i;
    int rc;

    for (i = 0u; i < token_len; ++i) {
        if (token[i] == '/') {
            slash = token + i;
        }
    }

    if (slash == 0) {
        dir_path[0] = '\0';
        shell_copy_string(prefix, SHELL_LINE_CAP, token);
    } else {
        uint32_t dir_len = (uint32_t)(slash - token);
        if (dir_len == 0u) {
            dir_path[0] = '/';
            dir_path[1] = '\0';
        } else {
            shell_memcpy(dir_path, token, dir_len);
            dir_path[dir_len] = '\0';
        }
        shell_copy_string(prefix, SHELL_LINE_CAP, slash + 1);
    }

    rc = fs_list_dir(dir_path, entries, SHELL_LIST_CAP, &count);
    if (rc != FS_OK) {
        return 0;
    }

    for (i = 0u; i < count; ++i) {
        if (shell_startswith(entries[i].name, prefix) == 0) {
            continue;
        }

        if (match_name != 0) {
            return 0;
        }

        match_name = entries[i].name;
        match_type = entries[i].type;
    }

    if (match_name == 0) {
        return 0;
    }

    completed[0] = '\0';
    if (slash != 0) {
        uint32_t copy_len = (uint32_t)((slash - token) + 1u);
        if (copy_len >= completed_cap) {
            return 0;
        }
        shell_memcpy(completed, token, copy_len);
        completed[copy_len] = '\0';
        shell_copy_string(completed + copy_len, completed_cap - copy_len, match_name);
    } else {
        shell_copy_string(completed, completed_cap, match_name);
    }

    if (match_type == FS_NODE_DIR) {
        uint32_t end_len = shell_strlen(completed);
        if ((end_len + 1u) < completed_cap) {
            completed[end_len] = '/';
            completed[end_len + 1u] = '\0';
        }
    }

    return 1;
}

static void shell_tab_hook(char* buf, uint32_t cap, uint32_t* len, uint32_t* cursor, void* ctx) {
    uint32_t token_start;
    uint32_t token_end;
    uint32_t i;
    uint32_t token_index = 0u;
    char token[SHELL_LINE_CAP];
    char replacement[SHELL_LINE_CAP];
    const char* command_match = 0;
    (void)ctx;

    if (*cursor == 0u || *cursor > *len) {
        return;
    }

    token_start = *cursor;
    while (token_start > 0u && shell_is_space(buf[token_start - 1u]) == 0) {
        --token_start;
    }

    token_end = *cursor;
    while (token_end < *len && shell_is_space(buf[token_end]) == 0) {
        ++token_end;
    }

    if (token_start == token_end) {
        return;
    }

    for (i = token_start; i < *cursor; ++i) {
        token[i - token_start] = buf[i];
    }
    token[*cursor - token_start] = '\0';

    if (token[0] == '\0') {
        return;
    }

    for (i = 0u; i < token_start; ++i) {
        if (shell_is_space(buf[i]) == 0 && (i == 0u || shell_is_space(buf[i - 1u]) != 0)) {
            ++token_index;
        }
    }

    if (token_index == 0u) {
        if (shell_find_unique_command(token, &command_match) == 0) {
            return;
        }
        (void)shell_replace_token(buf, cap, len, cursor, token_start, token_end, command_match);
        return;
    }

    if (shell_find_unique_path_match(token, replacement, SHELL_LINE_CAP) == 0) {
        return;
    }

    (void)shell_replace_token(buf, cap, len, cursor, token_start, token_end, replacement);
}

static void shell_put_u64_hex(uint64_t value) {
    static const char hex[] = "0123456789ABCDEF";
    int nibble;

    tty_puts("0x");
    for (nibble = 15; nibble >= 0; --nibble) {
        tty_putc(hex[(uint8_t)((value >> (nibble * 4)) & 0xFu)]);
    }
}

static void shell_put_u32_dec(uint32_t value) {
    char digits[10];
    uint32_t count = 0u;

    if (value == 0u) {
        tty_putc('0');
        return;
    }

    while (value != 0u && count < 10u) {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (count > 0u) {
        tty_putc(digits[--count]);
    }
}

static void shell_cmd_help(void) {
    tty_puts("help clear echo reboot shutdown ticks version pwd ls cd mkdir cat stat touch write rm exec mmstat\n");
}

static void shell_cmd_clear(void) {
    tty_clear();
    tty_set_color(TTY_WHITE);
}

static void shell_cmd_echo(char** argv, uint32_t argc) {
    uint32_t i;
    uint32_t first = 1u;
    uint8_t newline = 1u;

    while (first < argc) {
        const char* arg = argv[first];
        uint32_t j = 1u;

        if (shell_streq(arg, "--")) {
            ++first;
            break;
        }

        if (arg[0] != '-') {
            break;
        }

        if (arg[1] == '\0') {
            break;
        }

        while (arg[j] == 'n') {
            ++j;
        }

        if (arg[j] != '\0') {
            break;
        }

        newline = 0u;
        ++first;
    }

    for (i = first; i < argc; ++i) {
        if (i > first) {
            tty_putc(' ');
        }
        tty_puts(argv[i]);
    }

    if (newline != 0u) {
        tty_putc('\n');
    }
}

static void shell_flush_pending_changes(void) {
    uint8_t write_changes = 0u;
    int rc;

    if (fs_has_pending_changes() != 0u) {
        char answer[8];

        tty_puts("Write changes to boot media? [Y/n] ");
        tty_readline(answer, 8u);
        if (answer[0] == '\0' || answer[0] == ' ' || answer[0] == 'y' || answer[0] == 'Y') {
            write_changes = 1u;
        }
    }

    if (write_changes != 0u) {
        tty_puts("Writing...\n");
    }

    rc = fs_shutdown(write_changes);
    if (rc != FS_OK) {
        shell_print_fs_error("shutdown", rc);
    } else if (write_changes != 0u) {
        tty_puts("Write completed successfully.\n");
    }
}

static void shell_cmd_reboot(void) {
    tty_puts("Rebooting...\n");
    shell_flush_pending_changes();
    signal_raise(HW_RESET);
}

static void shell_cmd_shutdown(void) {
    tty_puts("Shutting down...\n");
    shell_flush_pending_changes();
    signal_raise(HW_PWR_DOWN);
}

static void shell_cmd_ticks(void) {
    uint64_t ticks = timer_ticks_get();

    tty_puts("ticks ");
    shell_put_u64_hex(ticks);
    tty_putc('\n');
}

static void shell_cmd_mmstat(void) {
    mm_print_stats();
}

static void shell_version(void){
	tty_puts("System/1 by Adava (AdavaSoftware) (C) 2026\n");
}

static void shell_print_fs_error(const char* cmd, int rc) {
    int err = fs_to_errno(rc);

    tty_set_color(TTY_RED);
    tty_puts(cmd);
    tty_puts(": ");
    tty_set_color(TTY_WHITE);

    if (err == POSIX_ENOENT) {
        tty_puts("path not found\n");
        return;
    }

    if (err == POSIX_EEXIST) {
        tty_puts("already exists\n");
        return;
    }

    if (err == POSIX_ENOTDIR) {
        tty_puts("not a directory\n");
        return;
    }

    if (err == POSIX_EINVAL) {
        tty_puts("invalid path\n");
        return;
    }

    if (err == POSIX_ENOSPC) {
        tty_puts("no space left in filesystem\n");
        return;
    }

    if (err == POSIX_EROFS) {
        tty_puts("read-only filesystem\n");
        return;
    }

    tty_puts("unknown fs error\n");
}

static void shell_print_usage(const char* cmd, const char* usage) {
    tty_set_color(TTY_RED);
    tty_puts(cmd);
    tty_puts(": ");
    tty_set_color(TTY_WHITE);
    tty_puts(usage);
    tty_putc('\n');
}

static void shell_print_posix_path_error(const char* cmd, const char* path, int rc) {
    int err = (rc < 0) ? -rc : rc;

    tty_set_color(TTY_RED);
    tty_puts(cmd);
    tty_puts(": ");
    tty_set_color(TTY_WHITE);
    tty_puts(path);
    tty_puts(": ");

    if (err == POSIX_ENOENT) {
        tty_puts("No such file or directory\n");
        return;
    }

    if (err == POSIX_ENOTDIR || err == POSIX_EISDIR) {
        tty_puts("Is a directory\n");
        return;
    }

    if (err == POSIX_EBADF) {
        tty_puts("Bad file descriptor\n");
        return;
    }

    if (err == POSIX_ENOEXEC) {
        tty_puts("Exec format error\n");
        return;
    }

    if (err == POSIX_ENOSYS) {
        tty_puts("Function not implemented\n");
        return;
    }

    if (err == POSIX_EINVAL) {
        tty_puts("Invalid argument\n");
        return;
    }

    if (err == POSIX_EIO) {
        tty_puts("Input/output error\n");
        return;
    }

    tty_puts("Error\n");
}

static void shell_cmd_pwd(char** argv, uint32_t argc) {
    uint32_t i;

    for (i = 1u; i < argc; ++i) {
        if (shell_streq(argv[i], "--")) {
            if ((i + 1u) < argc) {
                shell_print_usage("pwd", "too many operands");
                return;
            }
            break;
        }

        if (shell_streq(argv[i], "-L") || shell_streq(argv[i], "-P")) {
            continue;
        }

        shell_print_usage("pwd", "usage: pwd [-L|-P]");
        return;
    }

    tty_puts(fs_get_cwd_path());
    tty_putc('\n');
}

static void shell_ls_print_entry(const char* name, uint8_t is_dir) {
    tty_puts(name);
    if (is_dir != 0u) {
        tty_putc('/');
    }
    tty_putc('\n');
}

static void shell_ls_print_dir(const char* path, uint8_t show_all) {
    fs_dirent_t entries[SHELL_LIST_CAP];
    uint32_t count = 0u;
    uint32_t i;
    int rc = fs_list_dir(path, entries, SHELL_LIST_CAP, &count);

    if (rc != FS_OK) {
        shell_print_fs_error("ls", rc);
        return;
    }

    if (show_all != 0u) {
        shell_ls_print_entry(".", 1u);
        shell_ls_print_entry("..", 1u);
    }

    for (i = 0u; i < count; ++i) {
        if (show_all == 0u && entries[i].name[0] == '.') {
            continue;
        }
        shell_ls_print_entry(entries[i].name, (uint8_t)(entries[i].type == FS_NODE_DIR));
    }
}

static void shell_cmd_ls(char** argv, uint32_t argc) {
    uint8_t show_all = 0u;
    uint32_t first = 1u;
    uint32_t path_count;
    uint32_t i;

    while (first < argc) {
        const char* arg = argv[first];
        uint32_t j;

        if (shell_streq(arg, "--")) {
            ++first;
            break;
        }

        if (shell_is_option(arg) == 0) {
            break;
        }

        for (j = 1u; arg[j] != '\0'; ++j) {
            if (arg[j] == 'a' || arg[j] == '1') {
                if (arg[j] == 'a') {
                    show_all = 1u;
                }
                continue;
            }

            shell_print_usage("ls", "usage: ls [-a] [-1] [file ...]");
            return;
        }

        ++first;
    }

    path_count = argc - first;
    if (path_count == 0u) {
        shell_ls_print_dir("", show_all);
        return;
    }

    for (i = first; i < argc; ++i) {
        fs_stat_t st;
        int rc = posix_stat(argv[i], &st);

        if (path_count > 1u) {
            if (i > first) {
                tty_putc('\n');
            }
            tty_puts(argv[i]);
            tty_puts(":\n");
        }

        if (rc < 0) {
            shell_print_posix_path_error("ls", argv[i], rc);
            continue;
        }

        if (shell_is_directory_mode(st.mode) != 0) {
            shell_ls_print_dir(argv[i], show_all);
        } else {
            shell_ls_print_entry(argv[i], 0u);
        }
    }
}

static void shell_cmd_cd(char** argv, uint32_t argc) {
    const char* path = "/";
    char old_cwd[FS_PATH_CAP];
    uint8_t print_new_cwd = 0u;
    uint32_t first = 1u;
    int rc;

    shell_copy_string(old_cwd, FS_PATH_CAP, fs_get_cwd_path());

    while (first < argc) {
        if (shell_streq(argv[first], "--")) {
            ++first;
            break;
        }

        if (shell_streq(argv[first], "-L") || shell_streq(argv[first], "-P")) {
            ++first;
            continue;
        }

        break;
    }

    if (first < argc) {
        path = argv[first++];
    }

    if (first < argc) {
        shell_print_usage("cd", "usage: cd [-L|-P] [dir]");
        return;
    }

    if (shell_streq(path, "-")) {
        path = g_shell_prev_dir;
        print_new_cwd = 1u;
    }

    rc = fs_change_dir(path);
    if (rc != FS_OK) {
        shell_print_fs_error("cd", rc);
        return;
    }

    shell_copy_string(g_shell_prev_dir, FS_PATH_CAP, old_cwd);
    if (print_new_cwd != 0u) {
        tty_puts(fs_get_cwd_path());
        tty_putc('\n');
    }
}

static int shell_normalize_path(const char* path, char* out, uint32_t cap) {
    return fs_core_normalize_path(fs_get_cwd_path(), path, out, cap);
}

static int shell_mkdir_parents(const char* path) {
    char normalized[FS_PATH_CAP];
    uint32_t i;
    int rc;

    rc = shell_normalize_path(path, normalized, FS_PATH_CAP);
    if (rc != FS_OK) {
        return rc;
    }

    for (i = 1u; normalized[i] != '\0'; ++i) {
        if (normalized[i] != '/') {
            continue;
        }

        normalized[i] = '\0';
        rc = fs_make_dir(normalized);
        if (rc != FS_OK && fs_to_errno(rc) != POSIX_EEXIST) {
            normalized[i] = '/';
            return rc;
        }
        normalized[i] = '/';
    }

    rc = fs_make_dir(normalized);
    if (rc != FS_OK && fs_to_errno(rc) != POSIX_EEXIST) {
        return rc;
    }

    return FS_OK;
}

static void shell_cmd_mkdir(char** argv, uint32_t argc) {
    uint8_t create_parents = 0u;
    uint32_t first = 1u;
    uint32_t i;

    while (first < argc) {
        const char* arg = argv[first];

        if (shell_streq(arg, "--")) {
            ++first;
            break;
        }

        if (shell_streq(arg, "-p")) {
            create_parents = 1u;
            ++first;
            continue;
        }

        break;
    }

    if (first >= argc) {
        shell_print_usage("mkdir", "usage: mkdir [-p] dir ...");
        return;
    }

    for (i = first; i < argc; ++i) {
        int rc = (create_parents != 0u) ? shell_mkdir_parents(argv[i]) : fs_make_dir(argv[i]);
        if (rc != FS_OK) {
            shell_print_fs_error("mkdir", rc);
        }
    }
}

static void shell_cmd_cat(char** argv, uint32_t argc) {
    static char buffer[SHELL_CAT_BUF_CAP];
    uint32_t first = 1u;
    uint32_t argi;

    while (first < argc) {
        if (shell_streq(argv[first], "--")) {
            ++first;
            break;
        }

        if (shell_streq(argv[first], "-u")) {
            ++first;
            continue;
        }

        if (shell_is_option(argv[first]) != 0) {
            shell_print_usage("cat", "usage: cat [-u] [file ...]");
            return;
        }

        break;
    }

    if (first >= argc) {
        shell_print_usage("cat", "usage: cat [-u] [file ...]");
        return;
    }

    for (argi = first; argi < argc; ++argi) {
        int fd;
        int close_rc;

        fd = posix_open(argv[argi], FS_O_RDONLY);
        if (fd < 0) {
            shell_print_posix_path_error("cat", argv[argi], fd);
            continue;
        }

        for (;;) {
            int size = posix_read(fd, buffer, SHELL_CAT_BUF_CAP);
            if (size < 0) {
                shell_print_posix_path_error("cat", argv[argi], size);
                break;
            }

            if (size == 0) {
                break;
            }

            if (posix_write(POSIX_STDOUT_FILENO, buffer, (uint32_t)size) < 0) {
                shell_print_posix_path_error("cat", argv[argi], -POSIX_EBADF);
                break;
            }
        }

        close_rc = posix_close(fd);
        if (close_rc < 0) {
            shell_print_posix_path_error("cat", argv[argi], close_rc);
        }
    }
}

static void shell_cmd_stat(char** argv, uint32_t argc) {
    uint32_t first = 1u;
    uint32_t i;

    if (first < argc && shell_streq(argv[first], "--")) {
        ++first;
    }

    if (first >= argc) {
        shell_print_usage("stat", "usage: stat path ...");
        return;
    }

    for (i = first; i < argc; ++i) {
        fs_stat_t st;
        int rc = posix_stat(argv[i], &st);

        if (rc < 0) {
            shell_print_posix_path_error("stat", argv[i], rc);
            continue;
        }

        tty_puts(argv[i]);
        tty_puts(": mode ");
        tty_hex_u32(st.mode);
        tty_puts(" size ");
        shell_put_u32_dec(st.size);
        tty_putc('\n');
    }
}

static void shell_cmd_touch(char** argv, uint32_t argc) {
    uint32_t first = 1u;
    uint32_t i;

    if (first < argc && shell_streq(argv[first], "--")) {
        ++first;
    }

    if (first >= argc) {
        shell_print_usage("touch", "usage: touch file ...");
        return;
    }

    for (i = first; i < argc; ++i) {
        int fd = posix_open(argv[i], FS_O_CREAT | FS_O_RDWR);
        int rc;

        if (fd < 0) {
            shell_print_posix_path_error("touch", argv[i], fd);
            continue;
        }

        rc = posix_close(fd);
        if (rc < 0) {
            shell_print_posix_path_error("touch", argv[i], rc);
        }
    }
}

static void shell_cmd_write(char** argv, uint32_t argc) {
    uint32_t i;
    uint32_t first = 1u;
    uint8_t append = 0u;
    uint8_t newline = 1u;
    int fd;
    int rc;

    while (first < argc) {
        if (shell_streq(argv[first], "--")) {
            ++first;
            break;
        }

        if (shell_streq(argv[first], "-a")) {
            append = 1u;
            ++first;
            continue;
        }

        if (shell_streq(argv[first], "-n")) {
            newline = 0u;
            ++first;
            continue;
        }

        break;
    }

    if ((first + 1u) >= argc) {
        shell_print_usage("write", "usage: write [-a] [-n] file text...");
        return;
    }

    fd = posix_open(argv[first], FS_O_CREAT | (append != 0u ? FS_O_APPEND : FS_O_TRUNC) | FS_O_WRONLY);
    if (fd < 0) {
        shell_print_posix_path_error("write", argv[first], fd);
        return;
    }

    for (i = first + 1u; i < argc; ++i) {
        if (i > (first + 1u)) {
            rc = posix_write(fd, " ", 1u);
            if (rc < 0) {
                shell_print_posix_path_error("write", argv[first], rc);
                (void)posix_close(fd);
                return;
            }
        }

        rc = posix_write(fd, argv[i], shell_strlen(argv[i]));
        if (rc < 0) {
            shell_print_posix_path_error("write", argv[first], rc);
            (void)posix_close(fd);
            return;
        }
    }

    if (newline != 0u) {
        rc = posix_write(fd, "\n", 1u);
        if (rc < 0) {
            shell_print_posix_path_error("write", argv[first], rc);
            (void)posix_close(fd);
            return;
        }
    }

    rc = posix_close(fd);
    if (rc < 0) {
        shell_print_posix_path_error("write", argv[first], rc);
    }
}

static void shell_cmd_rm(char** argv, uint32_t argc) {
    uint8_t force = 0u;
    uint32_t first = 1u;
    uint32_t i;

    while (first < argc) {
        if (shell_streq(argv[first], "--")) {
            ++first;
            break;
        }

        if (shell_streq(argv[first], "-f")) {
            force = 1u;
            ++first;
            continue;
        }

        break;
    }

    if (first >= argc) {
        shell_print_usage("rm", "usage: rm [-f] file ...");
        return;
    }

    for (i = first; i < argc; ++i) {
        int rc = posix_unlink(argv[i]);
        if (rc < 0) {
            if (force != 0u && -rc == POSIX_ENOENT) {
                continue;
            }
            shell_print_posix_path_error("rm", argv[i], rc);
        }
    }
}

static void shell_cmd_exec(char** argv, uint32_t argc) {
    uint32_t first = 1u;
    int rc;

    if (first < argc && shell_streq(argv[first], "--")) {
        ++first;
    }

    if ((first + 1u) != argc) {
        shell_print_usage("exec", "usage: exec file.prg");
        return;
    }

    rc = posix_execve(argv[first], 0, 0);
    if (rc < 0) {
        shell_print_posix_path_error("exec", argv[first], rc);
    }
}

static void shell_print_prompt(void) {
    tty_puts(fs_get_cwd_path());
    tty_puts(" > ");
}

void shell_core_run(void) {
	klog_info("shell", "Starting shell...\n");
	tty_set_color(TTY_WHITE);
    uint16_t row = 0u;
    uint16_t col = 0u;
    tty_get_cursor(&row, &col);
    tty_text_begin(row, col);
    char line[SHELL_LINE_CAP];
    char history_line[SHELL_LINE_CAP];
    char* argv[SHELL_ARGV_MAX];
    uint32_t argc;
    uint32_t i;
    int tokenize_rc;
    tty_readline_hooks_t hooks;

    hooks.tab_hook = shell_tab_hook;
    hooks.history_hook = shell_history_hook;
    hooks.ctx = 0;

    for (;;) {
        shell_history_reset_line_state();
        shell_print_prompt();
        tty_readline_ex(line, SHELL_LINE_CAP, &hooks);
        for (i = 0u; i < SHELL_LINE_CAP; ++i) {
            history_line[i] = line[i];
            if (line[i] == '\0') {
                break;
            }
        }
        tokenize_rc = shell_tokenize(line, argv, SHELL_ARGV_MAX, &argc);

        if (tokenize_rc == -1) {
            tty_puts("syntax error: unterminated quote or escape\n");
            continue;
        }

        if (tokenize_rc == -2) {
            tty_puts("too many arguments\n");
            continue;
        }

        if (argc == 0u) {
            continue;
        }

        shell_history_push(history_line);

        if (shell_streq(argv[0], "help")) {
            shell_cmd_help();
            continue;
        }

        if (shell_streq(argv[0], "clear")) {
            shell_cmd_clear();
            continue;
        }

        if (shell_streq(argv[0], "echo")) {
            shell_cmd_echo(argv, argc);
            continue;
        }

        if (shell_streq(argv[0], "reboot")) {
            shell_cmd_reboot();
            continue;
        }

        if (shell_streq(argv[0], "shutdown")) {
            shell_cmd_shutdown();
            continue;
        }

        if (shell_streq(argv[0], "ticks")) {
            shell_cmd_ticks();
            continue;
        }
        
        if (shell_streq(argv[0], "version")){
			shell_version();
			continue;
		}

        if (shell_streq(argv[0], "pwd")) {
            shell_cmd_pwd(argv, argc);
            continue;
        }

        if (shell_streq(argv[0], "ls")) {
            shell_cmd_ls(argv, argc);
            continue;
        }

        if (shell_streq(argv[0], "cd")) {
            shell_cmd_cd(argv, argc);
            continue;
        }

        if (shell_streq(argv[0], "mkdir")) {
            shell_cmd_mkdir(argv, argc);
            continue;
        }

        if (shell_streq(argv[0], "cat")) {
            shell_cmd_cat(argv, argc);
            continue;
        }

        if (shell_streq(argv[0], "stat")) {
            shell_cmd_stat(argv, argc);
            continue;
        }

        if (shell_streq(argv[0], "touch")) {
            shell_cmd_touch(argv, argc);
            continue;
        }

        if (shell_streq(argv[0], "write")) {
            shell_cmd_write(argv, argc);
            continue;
        }

        if (shell_streq(argv[0], "rm")) {
            shell_cmd_rm(argv, argc);
            continue;
        }

        if (shell_streq(argv[0], "exec")) {
            shell_cmd_exec(argv, argc);
            continue;
        }

        if (shell_streq(argv[0], "mmstat")) {
            shell_cmd_mmstat();
            continue;
        }

		tty_set_color(TTY_RED);
        tty_puts("unknown command: ");
        tty_set_color(TTY_WHITE);
        tty_puts(argv[0]);
        tty_puts("\n");
    }
}
