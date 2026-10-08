#include "system1/dirent.h"
#include "system1/stat.h"
#include "system1/unistd.h"
#include "../common/uutil.h"

#define LS_ENTRY_CAP 32u

static int ls_directory(const char* path) {
    struct dirent entries[LS_ENTRY_CAP];
    int count;
    int i;

    count = getdents(path, entries, LS_ENTRY_CAP);
    if (count < 0) {
        u_err("ls: cannot read directory: ");
        u_err(path);
        u_err("\n");
        return 1;
    }

    for (i = 0; i < count; ++i) {
        u_puts(entries[i].d_name);
        if (entries[i].d_type == DT_DIR) u_puts("/");
        else if (entries[i].d_type == DT_LNK) u_puts("@");
        u_puts("\n");
    }

    return 0;
}

static int ls_path(const char* path, int print_header) {
    struct stat st;

    if (stat(path, &st) < 0) {
        u_err("ls: cannot access: ");
        u_err(path);
        u_err("\n");
        return 1;
    }

    if ((st.st_mode & S_IFDIR) == S_IFDIR) {
        if (print_header) {
            u_puts(path);
            u_puts(":\n");
        }
        return ls_directory(path);
    }

    u_puts(path);
    u_puts("\n");
    return 0;
}

int main(int argc, char** argv, char** envp) {
    int i;
    int rc = 0;
    int multiple = (argc > 2);
    (void)envp;

    if (argc <= 1) {
        return ls_directory(".");
    }

    for (i = 1; i < argc; ++i) {
        int current;

        if (multiple && i > 1) u_puts("\n");
        current = ls_path(argv[i], multiple);
        if (current != 0) rc = current;
    }

    return rc;
}
