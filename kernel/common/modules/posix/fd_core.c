#include "fd_core.h"
#include "fs_core.h"
#include "posix.h"
#include "tty.h"

typedef enum {
    FD_KIND_UNUSED = 0,
    FD_KIND_TTY_IN = 1,
    FD_KIND_TTY_OUT = 2,
    FD_KIND_VFS = 3
} fd_kind_t;

typedef struct {
    uint8_t used;
    fd_kind_t kind;
    uint32_t node_id;
    uint32_t offset;
    uint32_t flags;
} fd_entry_t;

static fd_entry_t g_fds[POSIX_FD_CAP];
static uint8_t g_fd_initialized = 0u;

static void fd_zero_entry(fd_entry_t* entry) {
    entry->used = 0u;
    entry->kind = FD_KIND_UNUSED;
    entry->node_id = 0u;
    entry->offset = 0u;
    entry->flags = 0u;
}

void fd_core_init(void) {
    uint32_t i;

    for (i = 0u; i < POSIX_FD_CAP; ++i) {
        fd_zero_entry(&g_fds[i]);
    }

    g_fds[POSIX_STDIN_FILENO].used = 1u;
    g_fds[POSIX_STDIN_FILENO].kind = FD_KIND_TTY_IN;

    g_fds[POSIX_STDOUT_FILENO].used = 1u;
    g_fds[POSIX_STDOUT_FILENO].kind = FD_KIND_TTY_OUT;

    g_fds[POSIX_STDERR_FILENO].used = 1u;
    g_fds[POSIX_STDERR_FILENO].kind = FD_KIND_TTY_OUT;

    g_fd_initialized = 1u;
}

static void fd_core_ensure_init(void) {
    if (g_fd_initialized == 0u) {
        fd_core_init();
    }
}

static int fd_to_errno(int rc) {
    int err;

    if (rc == FS_OK) {
        return 0;
    }

    err = fs_core_to_errno(rc);
    if (err == 0) {
        return -POSIX_EIO;
    }

    return -err;
}

static int fd_valid(int fd) {
    if (fd < 0 || fd >= (int)POSIX_FD_CAP) {
        return 0;
    }

    return (int)(g_fds[fd].used != 0u);
}

int fd_core_open(const char* path, uint32_t flags) {
    uint32_t node_id = 0u;
    uint32_t i;
    int rc;

    fd_core_ensure_init();

    rc = fs_core_open(path, flags, &node_id);
    if (rc != FS_OK) {
        return fd_to_errno(rc);
    }

    for (i = 3u; i < POSIX_FD_CAP; ++i) {
        if (g_fds[i].used == 0u) {
            g_fds[i].used = 1u;
            g_fds[i].kind = FD_KIND_VFS;
            g_fds[i].node_id = node_id;
            g_fds[i].flags = flags;
            g_fds[i].offset = 0u;

            if ((flags & FS_O_APPEND) != 0u) {
                uint32_t size = 0u;
                rc = fs_core_size(node_id, &size);
                if (rc != FS_OK) {
                    fd_zero_entry(&g_fds[i]);
                    (void)fs_core_close(node_id);
                    return fd_to_errno(rc);
                }
                g_fds[i].offset = size;
            }

            return (int)i;
        }
    }

    (void)fs_core_close(node_id);
    return -POSIX_ENOSPC;
}

int fd_core_close(int fd) {
    int rc;

    fd_core_ensure_init();

    if (fd < 3 || fd_valid(fd) == 0) {
        return -POSIX_EBADF;
    }

    if (g_fds[fd].kind == FD_KIND_VFS) {
        rc = fs_core_close(g_fds[fd].node_id);
        if (rc != FS_OK) {
            return fd_to_errno(rc);
        }
    }

    fd_zero_entry(&g_fds[fd]);
    return 0;
}

int fd_core_read(int fd, void* buffer, uint32_t count) {
    fd_entry_t* entry;
    uint32_t size = 0u;
    int rc;

    fd_core_ensure_init();

    if (fd_valid(fd) == 0 || buffer == 0) {
        return -POSIX_EBADF;
    }

    entry = &g_fds[fd];
    if (entry->kind == FD_KIND_TTY_IN) {
        return tty_readline((char*)buffer, count);
    }

    if (entry->kind != FD_KIND_VFS) {
        return -POSIX_EBADF;
    }

    rc = fs_core_read(entry->node_id, entry->offset, (char*)buffer, count, &size);
    if (rc != FS_OK) {
        return fd_to_errno(rc);
    }

    entry->offset += size;
    return (int)size;
}

int fd_core_write(int fd, const void* buffer, uint32_t count) {
    fd_entry_t* entry;
    uint32_t written = 0u;
    uint32_t i;
    int rc;

    fd_core_ensure_init();

    if (fd_valid(fd) == 0 || buffer == 0) {
        return -POSIX_EBADF;
    }

    entry = &g_fds[fd];
    if (entry->kind == FD_KIND_TTY_OUT) {
        const char* bytes = (const char*)buffer;
        for (i = 0u; i < count; ++i) {
            tty_putc(bytes[i]);
        }
        return (int)count;
    }

    if (entry->kind != FD_KIND_VFS) {
        return -POSIX_EBADF;
    }

    if ((entry->flags & FS_O_APPEND) != 0u) {
        uint32_t file_size = 0u;
        rc = fs_core_size(entry->node_id, &file_size);
        if (rc != FS_OK) {
            return fd_to_errno(rc);
        }
        entry->offset = file_size;
    }

    rc = fs_core_write(entry->node_id, entry->offset, (const char*)buffer, count, &written);
    if (rc != FS_OK) {
        return fd_to_errno(rc);
    }

    entry->offset += written;
    return (int)written;
}

int fd_core_lseek(int fd, int offset, uint32_t whence) {
    fd_entry_t* entry;
    uint32_t file_size = 0u;
    int base;
    int next;
    int rc;

    fd_core_ensure_init();

    if (fd_valid(fd) == 0 || g_fds[fd].kind != FD_KIND_VFS) {
        return -POSIX_EBADF;
    }

    entry = &g_fds[fd];
    if (whence == FS_SEEK_SET) {
        base = 0;
    } else if (whence == FS_SEEK_CUR) {
        base = (int)entry->offset;
    } else if (whence == FS_SEEK_END) {
        rc = fs_core_size(entry->node_id, &file_size);
        if (rc != FS_OK) {
            return fd_to_errno(rc);
        }
        base = (int)file_size;
    } else {
        return -POSIX_EINVAL;
    }

    next = base + offset;
    if (next < 0) {
        return -POSIX_EINVAL;
    }

    entry->offset = (uint32_t)next;
    return next;
}

int fd_core_fstat(int fd, fs_stat_t* out_stat) {
    int rc;

    fd_core_ensure_init();

    if (fd_valid(fd) == 0 || out_stat == 0 || g_fds[fd].kind != FD_KIND_VFS) {
        return -POSIX_EBADF;
    }

    rc = fs_core_fstat(g_fds[fd].node_id, out_stat);
    if (rc != FS_OK) {
        return fd_to_errno(rc);
    }

    return 0;
}
