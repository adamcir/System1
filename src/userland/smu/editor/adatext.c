#include "system1/errno.h"
#include "system1/fcntl.h"
#include "system1/unistd.h"
#include "../common/smu.h"
#include "../common/uutil.h"

#define ADATEXT_VERSION "0.1.0"
#define ADATEXT_BUFFER_CAP 3072u
#define ADATEXT_INPUT_CAP 256u
#define ADATEXT_PATH_CAP 96u

static char g_text[ADATEXT_BUFFER_CAP];
static unsigned g_text_len;
static char g_path[ADATEXT_PATH_CAP];
static int g_dirty;

static void put_u32(unsigned value) {
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

static char* skip_spaces(char* p) {
    while (*p == ' ' || *p == '\t') ++p;
    return p;
}

static int parse_number(char** text, unsigned* out) {
    char* p = skip_spaces(*text);
    unsigned value = 0u;
    int have_digit = 0;

    while (*p >= '0' && *p <= '9') {
        have_digit = 1;
        value = value * 10u + (unsigned)(*p - '0');
        ++p;
    }

    if (!have_digit) return -1;
    *text = p;
    *out = value;
    return 0;
}

static unsigned line_count(void) {
    unsigned count = 0u;
    unsigned i;

    if (g_text_len == 0u) return 0u;
    count = 1u;

    for (i = 0u; i < g_text_len; ++i) {
        if (g_text[i] == '\n' && i + 1u < g_text_len) ++count;
    }
    return count;
}

static int line_bounds(unsigned line, unsigned* start, unsigned* end, unsigned* remove_end) {
    unsigned current = 1u;
    unsigned i = 0u;

    if (line == 0u || g_text_len == 0u) return -1;

    while (current < line && i < g_text_len) {
        if (g_text[i++] == '\n') ++current;
    }

    if (current != line || i >= g_text_len) return -1;

    *start = i;
    while (i < g_text_len && g_text[i] != '\n') ++i;
    *end = i;
    *remove_end = (i < g_text_len && g_text[i] == '\n') ? i + 1u : i;
    return 0;
}

static void print_line(unsigned line) {
    unsigned start;
    unsigned end;
    unsigned remove_end;
    unsigned i;

    if (line_bounds(line, &start, &end, &remove_end) != 0) {
        u_err("adatext: line does not exist\n");
        return;
    }

    put_u32(line);
    u_puts(" | ");
    for (i = start; i < end; ++i) {
        (void)write(STDOUT_FILENO, &g_text[i], 1u);
    }
    u_puts("\n");
}

static void print_all(void) {
    unsigned count = line_count();
    unsigned i;

    if (count == 0u) {
        u_puts("[empty]\n");
        return;
    }

    for (i = 1u; i <= count; ++i) print_line(i);
}

static int insert_bytes(unsigned at, const char* data, unsigned size) {
    unsigned i;

    if (at > g_text_len || g_text_len + size >= ADATEXT_BUFFER_CAP) {
        u_err("adatext: buffer full\n");
        return -1;
    }

    for (i = g_text_len + 1u; i > at; --i) {
        g_text[i + size - 1u] = g_text[i - 1u];
    }

    for (i = 0u; i < size; ++i) g_text[at + i] = data[i];
    g_text_len += size;
    g_text[g_text_len] = '\0';
    g_dirty = 1;
    return 0;
}

static int insert_line(unsigned line, const char* text) {
    char record[ADATEXT_INPUT_CAP + 1u];
    unsigned count = line_count();
    unsigned at = 0u;
    unsigned len = u_strlen(text);
    unsigned i;

    if (line == 0u || line > count + 1u) {
        u_err("adatext: invalid line number\n");
        return -1;
    }
    if (len + 1u >= sizeof(record)) {
        u_err("adatext: line too long\n");
        return -1;
    }

    if (line <= count) {
        unsigned end;
        unsigned remove_end;
        if (line_bounds(line, &at, &end, &remove_end) != 0) return -1;
    } else {
        at = g_text_len;
    }

    for (i = 0u; i < len; ++i) record[i] = text[i];
    record[len++] = '\n';
    return insert_bytes(at, record, len);
}

static int append_line(const char* text) {
    return insert_line(line_count() + 1u, text);
}

static int delete_line(unsigned line) {
    unsigned start;
    unsigned end;
    unsigned remove_end;
    unsigned i;
    unsigned size;

    if (line_bounds(line, &start, &end, &remove_end) != 0) {
        u_err("adatext: line does not exist\n");
        return -1;
    }

    size = remove_end - start;
    for (i = remove_end; i <= g_text_len; ++i) {
        g_text[i - size] = g_text[i];
    }
    g_text_len -= size;
    g_dirty = 1;
    return 0;
}

static int replace_line(unsigned line, const char* text) {
    if (delete_line(line) != 0) return -1;
    return insert_line(line, text);
}

static int load_file(const char* path) {
    int fd;
    int n;
    int extra;
    char ch;

    g_text_len = 0u;
    g_text[0] = '\0';

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {
            if (u_copy(g_path, sizeof(g_path), path) != 0) {
                u_err("adatext: path too long\n");
                return -1;
            }
            g_dirty = 0;
            u_puts("AdaText: new file ");
            u_puts(path);
            u_puts("\n");
            return 0;
        }
        u_err("adatext: cannot open file\n");
        return -1;
    }

    n = read(fd, g_text, ADATEXT_BUFFER_CAP - 1u);
    if (n < 0) {
        (void)close(fd);
        u_err("adatext: read failed\n");
        return -1;
    }

    g_text_len = (unsigned)n;
    g_text[g_text_len] = '\0';

    extra = read(fd, &ch, 1u);
    (void)close(fd);
    if (extra > 0) {
        g_text_len = 0u;
        g_text[0] = '\0';
        u_err("adatext: file is larger than editor buffer\n");
        return -1;
    }

    if (u_copy(g_path, sizeof(g_path), path) != 0) {
        u_err("adatext: path too long\n");
        return -1;
    }

    g_dirty = 0;
    return 0;
}

static int save_file(const char* path) {
    int fd;
    unsigned done = 0u;

    if (path == 0 || path[0] == '\0') {
        u_err("adatext: no file name; use w <path>\n");
        return -1;
    }

    fd = open(path, O_CREAT | O_TRUNC | O_WRONLY);
    if (fd < 0) {
        u_err("adatext: cannot save file\n");
        return -1;
    }

    while (done < g_text_len) {
        int n = write(fd, g_text + done, g_text_len - done);
        if (n <= 0) {
            (void)close(fd);
            u_err("adatext: write failed\n");
            return -1;
        }
        done += (unsigned)n;
    }

    if (close(fd) < 0) {
        u_err("adatext: close failed\n");
        return -1;
    }

    if (u_copy(g_path, sizeof(g_path), path) != 0) {
        u_err("adatext: path too long\n");
        return -1;
    }

    g_dirty = 0;
    u_puts("Saved ");
    u_puts(g_path);
    u_puts("\n");
    return 0;
}

static void print_status(void) {
    u_puts("File: ");
    if (g_path[0] != '\0') u_puts(g_path);
    else u_puts("[no file]");
    u_puts(" | lines: ");
    put_u32(line_count());
    u_puts(" | bytes: ");
    put_u32(g_text_len);
    u_puts(g_dirty ? " | modified\n" : " | saved\n");
}

static void print_help(void) {
    u_puts("AdaText commands:\n");
    u_puts("  p [N]        print all text or line N\n");
    u_puts("  a TEXT       append a line\n");
    u_puts("  i N TEXT     insert before line N\n");
    u_puts("  r N TEXT     replace line N\n");
    u_puts("  d N          delete line N\n");
    u_puts("  e PATH       open another file\n");
    u_puts("  w [PATH]     save / save as\n");
    u_puts("  wq [PATH]    save and quit\n");
    u_puts("  s            status\n");
    u_puts("  q            quit if saved\n");
    u_puts("  q!           quit without saving\n");
    u_puts("  h            help\n");
}

static int handle_command(char* line) {
    char* p = skip_spaces(line);
    char command;

    if (*p == '\0') return 0;
    command = *p++;

    if (command == 'h' && *skip_spaces(p) == '\0') {
        print_help();
        return 0;
    }

    if (command == 's' && *skip_spaces(p) == '\0') {
        print_status();
        return 0;
    }

    if (command == 'p') {
        unsigned number;
        p = skip_spaces(p);
        if (*p == '\0') print_all();
        else if (parse_number(&p, &number) == 0 && *skip_spaces(p) == '\0') print_line(number);
        else u_err("adatext: usage: p [line]\n");
        return 0;
    }

    if (command == 'a') {
        p = skip_spaces(p);
        (void)append_line(p);
        return 0;
    }

    if (command == 'i' || command == 'r') {
        unsigned number;
        if (parse_number(&p, &number) != 0) {
            u_err(command == 'i' ? "adatext: usage: i line text\n"
                                 : "adatext: usage: r line text\n");
            return 0;
        }
        p = skip_spaces(p);
        if (command == 'i') (void)insert_line(number, p);
        else (void)replace_line(number, p);
        return 0;
    }

    if (command == 'd') {
        unsigned number;
        if (parse_number(&p, &number) != 0 || *skip_spaces(p) != '\0') {
            u_err("adatext: usage: d line\n");
            return 0;
        }
        (void)delete_line(number);
        return 0;
    }

    if (command == 'e') {
        p = skip_spaces(p);
        if (*p == '\0') {
            u_err("adatext: usage: e path\n");
            return 0;
        }
        if (g_dirty) {
            u_err("adatext: unsaved changes; save first or use q!\n");
            return 0;
        }
        (void)load_file(p);
        return 0;
    }

    if (command == 'w') {
        int quit_after = 0;
        if (*p == 'q') {
            quit_after = 1;
            ++p;
        }
        p = skip_spaces(p);

        if (save_file(*p != '\0' ? p : g_path) == 0 && quit_after) return 1;
        return 0;
    }

    if (command == 'q') {
        p = skip_spaces(p);
        if (*p == '!' && p[1] == '\0') return 1;
        if (*p != '\0') {
            u_err("adatext: usage: q or q!\n");
            return 0;
        }
        if (g_dirty) {
            u_err("adatext: unsaved changes; use w, wq, or q!\n");
            return 0;
        }
        return 1;
    }

    u_err("adatext: unknown command; use h for help\n");
    return 0;
}

int main(int argc, char** argv, char** envp) {
    char input[ADATEXT_INPUT_CAP];
    (void)envp;

    g_path[0] = '\0';
    g_text[0] = '\0';
    g_text_len = 0u;
    g_dirty = 0;

    if (argc > 2) {
        u_err("usage: adatext [file]\n");
        return 1;
    }

    if (argc == 2 && load_file(argv[1]) != 0) return 1;

    u_puts("AdaText ");
    u_puts(ADATEXT_VERSION);
    u_puts(" - ");
    u_puts(SMU_NAME);
    u_puts("\n");
    print_status();
    u_puts("Type h for help.\n");

    for (;;) {
        int n;

        u_puts("adatext> ");
        n = read(STDIN_FILENO, input, sizeof(input) - 1u);
        if (n < 0) {
            u_err("adatext: input failed\n");
            return 1;
        }
        if (n == 0) continue;

        input[n] = '\0';
        if (handle_command(input)) break;
    }

    return 0;
}
