#include "fd_core.h"
#include "fs_core.h"
#include "keyboard.h"
#include "signals.h"
#include "posix.h"
#include "process_core.h"
#include "tty.h"

typedef struct {
    uint8_t used;
    uint16_t refs;
    fd_kind_t kind;
    uint32_t node_id;
    uint32_t offset;
    uint32_t flags;
} fd_description_t;

typedef struct {
    const char* buffer;
    uint32_t len;
    uint32_t cursor;
} fd_tty_line_t;

static fd_description_t g_descriptions[POSIX_OFD_CAP];
static uint8_t g_descriptions_initialized;
static uint16_t g_tty_line_row;
static uint16_t g_tty_line_col;

static void fd_description_zero(fd_description_t* desc) {
    desc->used = 0u;
    desc->refs = 0u;
    desc->kind = FD_KIND_UNUSED;
    desc->node_id = 0u;
    desc->offset = 0u;
    desc->flags = 0u;
}

static void fd_descriptions_ensure_init(void) {
    uint32_t i;

    if (g_descriptions_initialized != 0u) return;
    for (i = 0u; i < POSIX_OFD_CAP; ++i) {
        fd_description_zero(&g_descriptions[i]);
    }
    g_descriptions_initialized = 1u;
}

static void fd_zero_entry(fd_entry_t* entry) {
    entry->used = 0u;
    entry->description_index = 0u;
}

void fd_core_table_reset(fd_table_t* table) {
    uint32_t i;

    if (table == 0) return;
    for (i = 0u; i < POSIX_FD_CAP; ++i) fd_zero_entry(&table->entries[i]);
    table->initialized = 0u;
}

static int fd_alloc_description(fd_kind_t kind, uint32_t node_id,
                                uint32_t flags, uint32_t offset) {
    uint32_t i;

    fd_descriptions_ensure_init();
    for (i = 0u; i < POSIX_OFD_CAP; ++i) {
        if (g_descriptions[i].used == 0u) {
            g_descriptions[i].used = 1u;
            g_descriptions[i].refs = 1u;
            g_descriptions[i].kind = kind;
            g_descriptions[i].node_id = node_id;
            g_descriptions[i].offset = offset;
            g_descriptions[i].flags = flags;
            return (int)i;
        }
    }
    return -1;
}

static fd_description_t* fd_description(fd_table_t* table, int fd) {
    uint32_t index;

    if (table == 0 || fd < 0 || fd >= (int)POSIX_FD_CAP ||
        table->entries[fd].used == 0u) {
        return 0;
    }

    index = table->entries[fd].description_index;
    if (index >= POSIX_OFD_CAP || g_descriptions[index].used == 0u) return 0;
    return &g_descriptions[index];
}

static int fd_assign_new_description(fd_table_t* table, int fd,
                                     fd_kind_t kind, uint32_t node_id,
                                     uint32_t flags, uint32_t offset) {
    int index;

    if (table == 0 || fd < 0 || fd >= (int)POSIX_FD_CAP) return -POSIX_EBADF;
    index = fd_alloc_description(kind, node_id, flags, offset);
    if (index < 0) return -POSIX_ENOSPC;

    table->entries[fd].used = 1u;
    table->entries[fd].description_index = (uint8_t)index;
    return 0;
}

void fd_core_table_init(fd_table_t* table) {
    if (table == 0) return;

    fd_descriptions_ensure_init();
    fd_core_table_reset(table);

    if (fd_assign_new_description(table, POSIX_STDIN_FILENO,
                                  FD_KIND_TTY_IN, 0u, 0u, 0u) < 0 ||
        fd_assign_new_description(table, POSIX_STDOUT_FILENO,
                                  FD_KIND_TTY_OUT, 0u, 0u, 0u) < 0 ||
        fd_assign_new_description(table, POSIX_STDERR_FILENO,
                                  FD_KIND_TTY_OUT, 0u, 0u, 0u) < 0) {
        fd_core_table_destroy(table);
        return;
    }

    table->initialized = 1u;
}

static int fd_release_entry(fd_table_t* table, int fd) {
    fd_description_t* desc;
    uint32_t index;
    int rc = 0;

    desc = fd_description(table, fd);
    if (desc == 0) return -POSIX_EBADF;

    index = table->entries[fd].description_index;
    fd_zero_entry(&table->entries[fd]);

    if (desc->refs > 0u) --desc->refs;
    if (desc->refs != 0u) return 0;

    if (desc->kind == FD_KIND_VFS) {
        rc = fs_core_close(desc->node_id);
    }
    fd_description_zero(&g_descriptions[index]);

    if (rc != FS_OK) {
        int err = fs_core_to_errno(rc);
        return -(err != 0 ? err : POSIX_EIO);
    }
    return 0;
}

void fd_core_table_destroy(fd_table_t* table) {
    uint32_t i;

    if (table == 0) return;
    for (i = 0u; i < POSIX_FD_CAP; ++i) {
        if (table->entries[i].used != 0u) {
            (void)fd_release_entry(table, (int)i);
        }
    }
    fd_core_table_reset(table);
}

int fd_core_table_clone(fd_table_t* dst, const fd_table_t* src) {
    uint32_t i;

    if (dst == 0 || src == 0 || src->initialized == 0u) return -POSIX_EINVAL;
    fd_core_table_reset(dst);

    for (i = 0u; i < POSIX_FD_CAP; ++i) {
        uint32_t index;

        if (src->entries[i].used == 0u) continue;
        index = src->entries[i].description_index;
        if (index >= POSIX_OFD_CAP || g_descriptions[index].used == 0u) {
            fd_core_table_destroy(dst);
            return -POSIX_EBADF;
        }

        dst->entries[i] = src->entries[i];
        ++g_descriptions[index].refs;
    }

    dst->initialized = 1u;
    return 0;
}

static fd_table_t* fd_current_table(void) {
    process_t* current = process_core_current();
    return (current != 0) ? &current->fd_table : 0;
}

void fd_core_init(void) {
    fd_table_t* table = fd_current_table();
    if (table != 0 && table->initialized == 0u) fd_core_table_init(table);
}

static void fd_core_ensure_init(void) {
    fd_core_init();
}

static int fd_to_errno(int rc) {
    int err;

    if (rc == FS_OK) return 0;
    err = fs_core_to_errno(rc);
    return -(err != 0 ? err : POSIX_EIO);
}

static int fd_valid(fd_table_t* table, int fd) {
    return fd_description(table, fd) != 0;
}

static int fd_find_free(fd_table_t* table, int start) {
    int fd;

    if (table == 0) return -1;
    for (fd = start; fd < (int)POSIX_FD_CAP; ++fd) {
        if (table->entries[fd].used == 0u) return fd;
    }
    return -1;
}

int fd_core_open(const char* path, uint32_t flags) {
    fd_table_t* table;
    uint32_t node_id = 0u;
    uint32_t offset = 0u;
    int fd;
    int index;
    int rc;

    fd_core_ensure_init();
    table = fd_current_table();
    if (table == 0) return -POSIX_EBADF;

    rc = fs_core_open(path, flags, &node_id);
    if (rc != FS_OK) return fd_to_errno(rc);

    if ((flags & FS_O_APPEND) != 0u) {
        rc = fs_core_size(node_id, &offset);
        if (rc != FS_OK) {
            (void)fs_core_close(node_id);
            return fd_to_errno(rc);
        }
    }

    fd = fd_find_free(table, 0);
    if (fd < 0) {
        (void)fs_core_close(node_id);
        return -POSIX_ENOSPC;
    }

    index = fd_alloc_description(FD_KIND_VFS, node_id, flags, offset);
    if (index < 0) {
        (void)fs_core_close(node_id);
        return -POSIX_ENOSPC;
    }

    table->entries[fd].used = 1u;
    table->entries[fd].description_index = (uint8_t)index;
    return fd;
}

int fd_core_close(int fd) {
    fd_table_t* table;

    fd_core_ensure_init();
    table = fd_current_table();
    if (fd_valid(table, fd) == 0) return -POSIX_EBADF;
    return fd_release_entry(table, fd);
}

int fd_core_read(int fd, void* buffer, uint32_t count) {
    fd_table_t* table;
    fd_description_t* desc;
    uint32_t size = 0u;
    int rc;

    fd_core_ensure_init();
    table = fd_current_table();
    desc = fd_description(table, fd);

    if (desc == 0 || buffer == 0) return -POSIX_EBADF;
    if (count == 0u) return 0;

    if (desc->kind == FD_KIND_TTY_IN) {
        return tty_readline((char*)buffer, count);
    }

    if (desc->kind != FD_KIND_VFS) return -POSIX_EBADF;
    if ((desc->flags & FS_O_RDWR) == FS_O_WRONLY) return -POSIX_EBADF;

    rc = fs_core_read(desc->node_id, desc->offset, (char*)buffer, count, &size);
    if (rc != FS_OK) return fd_to_errno(rc);

    desc->offset += size;
    return (int)size;
}

int fd_core_write(int fd, const void* buffer, uint32_t count) {
    fd_table_t* table;
    fd_description_t* desc;
    uint32_t written = 0u;
    uint32_t i;
    int rc;

    fd_core_ensure_init();
    table = fd_current_table();
    desc = fd_description(table, fd);

    if (desc == 0 || buffer == 0) return -POSIX_EBADF;
    if (count == 0u) return 0;

    if (desc->kind == FD_KIND_TTY_OUT) {
        const char* bytes = (const char*)buffer;
        for (i = 0u; i < count; ++i) tty_putc(bytes[i]);
        return (int)count;
    }

    if (desc->kind != FD_KIND_VFS) return -POSIX_EBADF;
    if ((desc->flags & FS_O_RDWR) == FS_O_RDONLY) return -POSIX_EBADF;

    if ((desc->flags & FS_O_APPEND) != 0u) {
        uint32_t file_size = 0u;
        rc = fs_core_size(desc->node_id, &file_size);
        if (rc != FS_OK) return fd_to_errno(rc);
        desc->offset = file_size;
    }

    rc = fs_core_write(desc->node_id, desc->offset,
                       (const char*)buffer, count, &written);
    if (rc != FS_OK) return fd_to_errno(rc);

    desc->offset += written;
    return (int)written;
}

int fd_core_lseek(int fd, int offset, uint32_t whence) {
    fd_table_t* table;
    fd_description_t* desc;
    uint32_t file_size = 0u;
    int base;
    int next;
    int rc;

    fd_core_ensure_init();
    table = fd_current_table();
    desc = fd_description(table, fd);

    if (desc == 0 || desc->kind != FD_KIND_VFS) return -POSIX_EBADF;

    if (whence == FS_SEEK_SET) base = 0;
    else if (whence == FS_SEEK_CUR) base = (int)desc->offset;
    else if (whence == FS_SEEK_END) {
        rc = fs_core_size(desc->node_id, &file_size);
        if (rc != FS_OK) return fd_to_errno(rc);
        base = (int)file_size;
    } else return -POSIX_EINVAL;

    next = base + offset;
    if (next < 0) return -POSIX_EINVAL;

    desc->offset = (uint32_t)next;
    return next;
}

int fd_core_fstat(int fd, fs_stat_t* out_stat) {
    fd_table_t* table;
    fd_description_t* desc;
    int rc;

    fd_core_ensure_init();
    table = fd_current_table();
    desc = fd_description(table, fd);

    if (desc == 0 || out_stat == 0) return -POSIX_EBADF;

    if (desc->kind == FD_KIND_TTY_IN || desc->kind == FD_KIND_TTY_OUT) {
        out_stat->mode = 0020000u;
        out_stat->size = 0u;
        return 0;
    }

    if (desc->kind != FD_KIND_VFS) return -POSIX_EBADF;
    rc = fs_core_fstat(desc->node_id, out_stat);
    return (rc == FS_OK) ? 0 : fd_to_errno(rc);
}

int fd_core_dup(int oldfd) {
    fd_table_t* table;
    fd_description_t* desc;
    int newfd;

    fd_core_ensure_init();
    table = fd_current_table();
    desc = fd_description(table, oldfd);
    if (desc == 0) return -POSIX_EBADF;

    newfd = fd_find_free(table, 0);
    if (newfd < 0) return -POSIX_ENOSPC;

    table->entries[newfd] = table->entries[oldfd];
    ++desc->refs;
    return newfd;
}

int fd_core_dup2(int oldfd, int newfd) {
    fd_table_t* table;
    fd_description_t* desc;

    fd_core_ensure_init();
    table = fd_current_table();
    desc = fd_description(table, oldfd);

    if (desc == 0) return -POSIX_EBADF;
    if (newfd < 0 || newfd >= (int)POSIX_FD_CAP) return -POSIX_EBADF;
    if (oldfd == newfd) return newfd;

    if (table->entries[newfd].used != 0u) {
        int rc = fd_release_entry(table, newfd);
        if (rc < 0) return rc;
    }

    table->entries[newfd] = table->entries[oldfd];
    ++desc->refs;
    return newfd;
}

int fd_core_isatty(int fd) {
    fd_table_t* table;
    fd_description_t* desc;

    fd_core_ensure_init();
    table = fd_current_table();
    desc = fd_description(table, fd);
    if (desc == 0) return -POSIX_EBADF;

    return (desc->kind == FD_KIND_TTY_IN || desc->kind == FD_KIND_TTY_OUT) ? 1 : 0;
}

int fd_core_ioctl(int fd, uint32_t request, uint32_t arg) {
    fd_table_t* table;
    fd_description_t* desc;

    fd_core_ensure_init();
    table = fd_current_table();
    desc = fd_description(table, fd);

    if (desc == 0) return -POSIX_EBADF;
    if (desc->kind != FD_KIND_TTY_IN && desc->kind != FD_KIND_TTY_OUT) {
        return -POSIX_ENOTTY;
    }

    if (request == 0x5301u) {
        if (arg > (uint32_t)TTY_WHITE) return -POSIX_EINVAL;
        tty_set_color((tty_color_t)arg);
        return 0;
    }

    if (request == 0x5302u) {
        int key;

        if (desc->kind != FD_KIND_TTY_IN) return -POSIX_ENOTTY;
        for (;;) {
            keyboard_poll();
            key = keyboard_take_key();
            if (key == KEY_NONE) {
                __asm__ volatile ("hlt");
                continue;
            }
            if (key == KEY_CTRL_ALT_DEL) {
                signal_raise(HW_RESET);
                continue;
            }
            return key;
        }
    }

    if (request == 0x5303u) {
        if (desc->kind != FD_KIND_TTY_IN) return -POSIX_ENOTTY;
        tty_get_cursor(&g_tty_line_row, &g_tty_line_col);
        tty_text_begin(g_tty_line_row, g_tty_line_col);
        return 0;
    }

    if (request == 0x5304u) {
        const fd_tty_line_t* line = (const fd_tty_line_t*)(uintptr_t)arg;

        if (desc->kind != FD_KIND_TTY_IN || line == 0 || line->buffer == 0) {
            return -POSIX_EINVAL;
        }
        if (line->cursor > line->len) return -POSIX_EINVAL;

        tty_line_redraw(line->buffer, line->len, line->cursor,
                        g_tty_line_row, g_tty_line_col);
        return 0;
    }

    return -POSIX_ENOTTY;
}
