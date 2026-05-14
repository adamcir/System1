#include "shell_core.h"
#include "types.h"
#include "interrupts.h"
#include "signals.h"
#include "tty.h"
#include "vga.h"
#include "klog.h"
#include "fs.h"

#define SHELL_LINE_CAP 256u
#define SHELL_ARGV_MAX 16u
#define SHELL_HISTORY_CAP 16u
#define SHELL_LIST_CAP 16u

static const char* g_shell_commands[] = {
    "help", "clear", "echo", "reboot", "shutdown", "ticks",
    "version", "pwd", "ls", "cd", "mkdir"
};

static char g_shell_history[SHELL_HISTORY_CAP][SHELL_LINE_CAP];
static uint32_t g_shell_history_count;
static uint32_t g_shell_history_head;
static int g_shell_history_nav = -1;
static char g_shell_history_draft[SHELL_LINE_CAP];

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

static uint32_t shell_tokenize(char* line, char** argv, uint32_t argv_cap) {
    uint32_t argc = 0u;
    char* p = line;

    while (*p != '\0') {
        while (*p == ' ' || *p == '\t') {
            ++p;
        }

        if (*p == '\0') {
            break;
        }

        if (argc >= argv_cap) {
            break;
        }

        argv[argc++] = p;

        while (*p != '\0' && *p != ' ' && *p != '\t') {
            ++p;
        }

        if (*p == '\0') {
            break;
        }

        *p = '\0';
        ++p;
    }

    return argc;
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

static int shell_is_space(char c) {
    return (int)(c == ' ' || c == '\t');
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

    vga_puts("0x");
    for (nibble = 15; nibble >= 0; --nibble) {
        vga_putc(hex[(uint8_t)((value >> (nibble * 4)) & 0xFu)]);
    }
}

static void shell_cmd_help(void) {
    vga_puts("help clear echo reboot shutdown ticks version pwd ls cd mkdir\n");
}

static void shell_cmd_clear(void) {
    vga_init();
    vga_set_color(WHITE);
}

static void shell_cmd_echo(char** argv, uint32_t argc) {
    uint32_t i;

    for (i = 1u; i < argc; ++i) {
        if (i > 1u) {
            vga_putc(' ');
        }
        vga_puts(argv[i]);
    }
    vga_putc('\n');
}

static void shell_cmd_reboot(void) {
    vga_puts("Rebooting...\n");
    signal_raise(HW_RESET);
}

static void shell_cmd_shutdown(void) {
    vga_puts("Shutting down...\n");
    signal_raise(HW_PWR_DOWN);
}

static void shell_cmd_ticks(void) {
    uint64_t ticks = timer_ticks_get();

    vga_puts("ticks ");
    shell_put_u64_hex(ticks);
    vga_putc('\n');
}

static void shell_version(void){
	vga_puts("System/1 by Adava (AdavaSoftware) (C) 2026\n");
	vga_puts("All rights reserved.\n");
}

static void shell_print_fs_error(const char* cmd, int rc) {
    vga_set_color(RED);
    vga_puts(cmd);
    vga_puts(": ");
    vga_set_color(WHITE);

    if (rc == FS_ERR_NOT_FOUND) {
        vga_puts("path not found\n");
        return;
    }

    if (rc == FS_ERR_EXISTS) {
        vga_puts("already exists\n");
        return;
    }

    if (rc == FS_ERR_NOT_DIR) {
        vga_puts("not a directory\n");
        return;
    }

    if (rc == FS_ERR_INVALID) {
        vga_puts("invalid path\n");
        return;
    }

    if (rc == FS_ERR_NO_SPACE) {
        vga_puts("no space left in ramfs\n");
        return;
    }

    vga_puts("unknown fs error\n");
}

static void shell_cmd_pwd(void) {
    vga_puts(fs_get_cwd_path());
    vga_putc('\n');
}

static void shell_cmd_ls(char** argv, uint32_t argc) {
    fs_dirent_t entries[16];
    uint32_t count = 0u;
    uint32_t i;
    const char* path = "";
    int rc;

    if (argc > 1u) {
        path = argv[1];
    }

    rc = fs_list_dir(path, entries, 16u, &count);
    if (rc != FS_OK) {
        shell_print_fs_error("ls", rc);
        return;
    }

    for (i = 0u; i < count; ++i) {
        vga_puts(entries[i].name);
        if (entries[i].type == FS_NODE_DIR) {
            vga_putc('/');
        }
        vga_putc('\n');
    }
}

static void shell_cmd_cd(char** argv, uint32_t argc) {
    int rc;

    if (argc < 2u) {
        vga_puts("cd: missing path\n");
        return;
    }

    rc = fs_change_dir(argv[1]);
    if (rc != FS_OK) {
        shell_print_fs_error("cd", rc);
    }
}

static void shell_cmd_mkdir(char** argv, uint32_t argc) {
    int rc;

    if (argc < 2u) {
        vga_puts("mkdir: missing path\n");
        return;
    }

    rc = fs_make_dir(argv[1]);
    if (rc != FS_OK) {
        shell_print_fs_error("mkdir", rc);
    }
}

static void shell_print_prompt(void) {
    vga_puts(fs_get_cwd_path());
    vga_puts(" > ");
}

void shell_core_run(void) {
	klog_info("shell", "Starting shell...\n");
	vga_set_color(WHITE);
    uint16_t row = 0u;
    uint16_t col = 0u;
    vga_get_cursor(&row, &col);
    vga_text_begin(row, col);
    char line[SHELL_LINE_CAP];
    char history_line[SHELL_LINE_CAP];
    char* argv[SHELL_ARGV_MAX];
    uint32_t argc;
    uint32_t i;
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
        argc = shell_tokenize(line, argv, SHELL_ARGV_MAX);

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
            shell_cmd_pwd();
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

		vga_set_color(RED);
        vga_puts("unknown command: ");
        vga_set_color(WHITE);
        vga_puts(argv[0]);
        vga_puts("\n");
    }
}
