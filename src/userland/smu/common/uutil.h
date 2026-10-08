#ifndef SYSTEM1_USERLAND_UUTIL_H
#define SYSTEM1_USERLAND_UUTIL_H

#include "system1/unistd.h"

static unsigned u_strlen(const char* s) {
    unsigned n = 0u;
    if (s == 0) return 0u;
    while (s[n] != '\0') ++n;
    return n;
}

static int u_streq(const char* a, const char* b) {
    unsigned i = 0u;
    if (a == 0 || b == 0) return 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return 0;
        ++i;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static int u_has_slash(const char* s) {
    unsigned i = 0u;
    while (s != 0 && s[i] != '\0') {
        if (s[i] == '/') return 1;
        ++i;
    }
    return 0;
}

static int u_ends_prg(const char* s) {
    unsigned n = u_strlen(s);
    return n >= 4u && s[n-4u] == '.' && s[n-3u] == 'p' &&
           s[n-2u] == 'r' && s[n-1u] == 'g';
}

static void u_puts_fd(int fd, const char* s) {
    (void)write(fd, s, u_strlen(s));
}

static void u_puts(const char* s) {
    u_puts_fd(STDOUT_FILENO, s);
}

static void u_err(const char* s) {
    u_puts_fd(STDERR_FILENO, s);
}

static int u_copy(char* dst, unsigned cap, const char* src) {
    unsigned i = 0u;
    if (dst == 0 || cap == 0u || src == 0) return -1;
    while (src[i] != '\0') {
        if (i + 1u >= cap) return -1;
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
    return 0;
}

static int u_join3(char* dst, unsigned cap,
                   const char* a, const char* b, const char* c) {
    unsigned p = 0u;
    unsigned i;
    const char* parts[3];
    parts[0] = a; parts[1] = b; parts[2] = c;
    for (unsigned part = 0u; part < 3u; ++part) {
        if (parts[part] == 0) continue;
        for (i = 0u; parts[part][i] != '\0'; ++i) {
            if (p + 1u >= cap) return -1;
            dst[p++] = parts[part][i];
        }
    }
    dst[p] = '\0';
    return 0;
}

#endif
