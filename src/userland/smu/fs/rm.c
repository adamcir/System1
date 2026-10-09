#include "system1/unistd.h"
#include "system1/stat.h"
#include "system1/dirent.h"
#include "system1/errno.h"
#include "../common/uutil.h"

#define RM_DEPTH 16u
#define RM_PATH 128u
#define RM_ENTRIES 32u

static int rm_recursive(const char* path, unsigned depth, int recursive, int force) {
    struct stat st;
    if (stat(path, &st) < 0) {
        if (force && errno == ENOENT) return 0;
        u_err("rm: cannot access "); u_err(path); u_err("\n");
        return 1;
    }
    if ((st.st_mode & S_IFDIR) == S_IFDIR) {
        struct dirent entries[RM_ENTRIES];
        int n, i, failed = 0;
        if (!recursive) {
            u_err("rm: is a directory: "); u_err(path); u_err("\n"); return 1;
        }
        if (depth >= RM_DEPTH) {
            u_err("rm: maximum directory depth exceeded\n"); return 1;
        }
        n = getdents(path, entries, RM_ENTRIES);
        if (n < 0) {
            u_err("rm: cannot list "); u_err(path); u_err("\n"); return 1;
        }
        for (i = 0; i < n; ++i) {
            char child[RM_PATH];
            if (u_streq(entries[i].d_name, ".") || u_streq(entries[i].d_name, ".."))
                continue;
            if (u_join3(child, sizeof(child), path, "/", entries[i].d_name) != 0) {
                u_err("rm: path too long\n"); failed = 1; continue;
            }
            if (rm_recursive(child, depth + 1u, 1, force) != 0) failed = 1;
        }
        if (failed) return 1;
        if (rmdir(path) < 0) {
            u_err("rm: cannot remove directory "); u_err(path); u_err("\n");
            return 1;
        }
        return 0;
    }
    if (unlink(path) < 0) {
        if (force && errno == ENOENT) return 0;
        u_err("rm: cannot remove "); u_err(path); u_err("\n");
        return 1;
    }
    return 0;
}

static int unsafe_target(const char* p) {
    unsigned i = 0u;
    while (p && p[0] == '.' && p[1] == '/') p += 2;
    if (!p || !p[0] || u_streq(p, "/") || u_streq(p, ".") || u_streq(p, ".."))
        return 1;
    /* Protect the current directory, root and parent traversals.
     * Refuse special segments, including /./, /../ and trailing /..
     */
    while (p[i]) {
        unsigned start;
        while (p[i] == '/') ++i;
        if (!p[i]) break;
        start = i;
        while (p[i] && p[i] != '/') ++i;
        if ((i - start == 1u && p[start] == '.') ||
            (i - start == 2u && p[start] == '.' && p[start + 1u] == '.'))
            return 1;
    }
    for (i = 0u; p[i] == '/'; ++i) {}
    return p[i] == '\0';
}

int main(int argc, char** argv, char** envp) {
    int i, first = 1, recursive = 0, force = 0, rc = 0;
    (void)envp;
    while (first < argc) {
        const char* a = argv[first];
        unsigned j;
        if (u_streq(a, "--")) { ++first; break; }
        if (a[0] != '-' || !a[1]) break;
        for (j = 1u; a[j]; ++j) {
            if (a[j] == 'r' || a[j] == 'R') recursive = 1;
            else if (a[j] == 'f') force = 1;
            else {
                u_err("rm: usage: rm [-rRf] [--] path ...\n"); return 2;
            }
        }
        ++first;
    }
    if (first == argc) {
        if (force) return 0;
        u_err("rm: missing operand\n"); return 1;
    }
    for (i = first; i < argc; ++i) {
        char path[RM_PATH];
        unsigned n;
        if (u_copy(path, sizeof(path), argv[i]) != 0) {
            u_err("rm: path too long\n"); rc = 1; continue;
        }
        n = u_strlen(path);
        while (n > 1u && path[n - 1u] == '/') path[--n] = '\0';
        if (unsafe_target(path)) {
            u_err("rm: refusing unsafe path: "); u_err(argv[i]); u_err("\n");
            rc = 1;
            continue;
        }
        if (rm_recursive(path, 0u, recursive, force) != 0) rc = 1;
    }
    return rc;
}
