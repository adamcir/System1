#include "system1/dirent.h"
#include "system1/stat.h"
#include "system1/unistd.h"
#include "../common/uutil.h"

#define LS_CAP 32u
static int g_all, g_long, g_dir_only;

static void num(unsigned n) {
    char b[12];
    unsigned i = 0u;
    do { b[i++] = (char)('0' + n % 10u); n /= 10u; } while (n);
    while (i) (void)write(STDOUT_FILENO, &b[--i], 1u);
}
static void print_item(const char* name, unsigned char type, const char* dir) {
    if (g_long) {
        char path[128];
        struct stat st;
        if (dir && u_join3(path, sizeof(path), dir, "/", name) == 0 &&
            stat(path, &st) == 0) {
            u_puts(type == DT_DIR ? "d " : type == DT_LNK ? "l " : "- ");
            num(st.st_size); u_puts(" ");
        } else u_puts("? ");
    }
    u_puts(name);
    if (type == DT_DIR) u_puts("/");
    else if (type == DT_LNK) u_puts("@");
    u_puts("\n");
}
static int list_dir(const char* path) {
    struct dirent entries[LS_CAP];
    int count = getdents(path, entries, LS_CAP);
    int i;
    if (count < 0) {
        u_err("ls: cannot read "); u_err(path); u_err("\n"); return 1;
    }
    for (i = 0; i < count; ++i) {
        if (!g_all && entries[i].d_name[0] == '.') continue;
        print_item(entries[i].d_name, entries[i].d_type, path);
    }
    return 0;
}
static int list_path(const char* path, int header) {
    struct stat st;
    if (stat(path, &st) < 0) {
        u_err("ls: cannot access "); u_err(path); u_err("\n"); return 1;
    }
    if (header) { u_puts(path); u_puts(":\n"); }
    if ((st.st_mode & S_IFDIR) == S_IFDIR && !g_dir_only) return list_dir(path);
    print_item(path, (st.st_mode & S_IFDIR) == S_IFDIR ? DT_DIR : DT_REG, 0);
    return 0;
}
int main(int argc, char** argv, char** envp) {
    int first = 1, i, rc = 0;
    (void)envp;
    while (first < argc) {
        const char* a = argv[first];
        unsigned j;
        if (u_streq(a, "--")) { ++first; break; }
        if (a[0] != '-' || !a[1]) break;
        for (j = 1u; a[j]; ++j) {
            if (a[j] == 'a' || a[j] == 'A') g_all = 1;
            else if (a[j] == 'l') g_long = 1;
            else if (a[j] == 'd') g_dir_only = 1;
            else if (a[j] == '1' || a[j] == 'F') {}
            else { u_err("ls: usage: ls [-ald1F] [paths...]\n"); return 2; }
        }
        ++first;
    }
    if (first == argc) return list_dir(".");
    for (i = first; i < argc; ++i) {
        if (i > first) u_puts("\n");
        if (list_path(argv[i], argc - first > 1) != 0) rc = 1;
    }
    return rc;
}
