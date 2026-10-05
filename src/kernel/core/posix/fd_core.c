#include "fd_core.h"
#include "fs_core.h"
#include "keyboard.h"
#include "signals.h"
#include "posix.h"
#include "process_core.h"
#include "tty.h"

static uint16_t g_tty_line_row;
static uint16_t g_tty_line_col;

typedef struct {
    const char* buffer;
    uint32_t len;
    uint32_t cursor;
} fd_tty_line_t;

static void fd_zero_entry(fd_entry_t* entry) {
    entry->used = 0u;
    entry->kind = FD_KIND_UNUSED;
    entry->node_id = 0u;
    entry->offset = 0u;
    entry->flags = 0u;
}

void fd_core_table_init(fd_table_t* table) {
    uint32_t i;

    if (table == 0) {
        return;
    }

    for (i = 0u; i < POSIX_FD_CAP; ++i) {
        fd_zero_entry(&table->entries[i]);
    }

    table->entries[POSIX_STDIN_FILENO].used = 1u;
    table->entries[POSIX_STDIN_FILENO].kind = FD_KIND_TTY_IN;

    table->entries[POSIX_STDOUT_FILENO].used = 1u;
    table->entries[POSIX_STDOUT_FILENO].kind = FD_KIND_TTY_OUT;

    table->entries[POSIX_STDERR_FILENO].used = 1u;
    table->entries[POSIX_STDERR_FILENO].kind = FD_KIND_TTY_OUT;

    table->initialized = 1u;
}

static fd_table_t* fd_current_table(void) {
    process_t* current = process_core_current();

    if (current == 0) {
        return 0;
    }

    return &current->fd_table;
}

void fd_core_init(void) {
    fd_core_table_init(fd_current_table());
}

static void fd_core_ensure_init(void) {
    fd_table_t* table = fd_current_table();

    if (table != 0 && table->initialized == 0u) {
        fd_core_table_init(table);
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

static int fd_valid(fd_table_t* table, int fd) {
    if (fd < 0 || fd >= (int)POSIX_FD_CAP) {
        return 0;
    }

    if (table == 0) {
        return 0;
    }

    return (int)(table->entries[fd].used != 0u);
}

int fd_core_open(const char* path, uint32_t flags) {
    fd_table_t* table;
    uint32_t node_id = 0u;
    uint32_t i;
    int rc;

    fd_core_ensure_init();
    table = fd_current_table();
    if (table == 0) {
        return -POSIX_EBADF;
    }

    rc = fs_core_open(path, flags, &node_id);
    if (rc != FS_OK) {
        return fd_to_errno(rc);
    }

    for (i = 3u; i < POSIX_FD_CAP; ++i) {
        if (table->entries[i].used == 0u) {
            table->entries[i].used = 1u;
            table->entries[i].kind = FD_KIND_VFS;
            table->entries[i].node_id = node_id;
            table->entries[i].flags = flags;
            table->entries[i].offset = 0u;

            if ((flags & FS_O_APPEND) != 0u) {
                uint32_t size = 0u;
                rc = fs_core_size(node_id, &size);
                if (rc != FS_OK) {
                    fd_zero_entry(&table->entries[i]);
                    (void)fs_core_close(node_id);
                    return fd_to_errno(rc);
                }
                table->entries[i].offset = size;
            }

            return (int)i;
        }
    }

    (void)fs_core_close(node_id);
    return -POSIX_ENOSPC;
}

int fd_core_close(int fd) {
    fd_table_t* table;
    int rc;

    fd_core_ensure_init();
    table = fd_current_table();

    if (fd < 3 || fd_valid(table, fd) == 0) {
        return -POSIX_EBADF;
    }

    if (table->entries[fd].kind == FD_KIND_VFS) {
        rc = fs_core_close(table->entries[fd].node_id);
        if (rc != FS_OK) {
            return fd_to_errno(rc);
        }
    }

    fd_zero_entry(&table->entries[fd]);
    return 0;
}

int fd_core_read(int fd, void* buffer, uint32_t count) {
    fd_table_t* table;
    fd_entry_t* entry;
    uint32_t size = 0u;
    int rc;

    fd_core_ensure_init();
    table = fd_current_table();

    if (fd_valid(table, fd) == 0 || buffer == 0) {
        return -POSIX_EBADF;
    }

    entry = &table->entries[fd];
    if (entry->kind == FD_KIND_TTY_IN) {
        return tty_readline((char*)buffer, count);
    }

    if (entry->kind != FD_KIND_VFS) {
        return -POSIX_EBADF;
    }

    if ((entry->flags & FS_O_RDWR) == FS_O_WRONLY) {
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
    fd_table_t* table;
    fd_entry_t* entry;
    uint32_t written = 0u;
    uint32_t i;
    int rc;

    fd_core_ensure_init();
    table = fd_current_table();

    if (fd_valid(table, fd) == 0 || buffer == 0) {
        return -POSIX_EBADF;
    }

    entry = &table->entries[fd];
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

    if ((entry->flags & FS_O_RDWR) == FS_O_RDONLY) {
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
    fd_table_t* table;
    fd_entry_t* entry;
    uint32_t file_size = 0u;
    int base;
    int next;
    int rc;

    fd_core_ensure_init();
    table = fd_current_table();

    if (fd_valid(table, fd) == 0 || table->entries[fd].kind != FD_KIND_VFS) {
        return -POSIX_EBADF;
    }

    entry = &table->entries[fd];
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
    fd_table_t* table;
    int rc;

    fd_core_ensure_init();
    table = fd_current_table();

    if (fd_valid(table, fd) == 0 || out_stat == 0 || table->entries[fd].kind != FD_KIND_VFS) {
        return -POSIX_EBADF;
    }

    rc = fs_core_fstat(table->entries[fd].node_id, out_stat);
    if (rc != FS_OK) {
        return fd_to_errno(rc);
    }

    return 0;
}

int fd_core_ioctl(int fd, uint32_t request, uint32_t arg) {
    fd_table_t* table;
    fd_entry_t* entry;

    fd_core_ensure_init();
    table = fd_current_table();

    if (fd_valid(table, fd) == 0) return -POSIX_EBADF;
    entry = &table->entries[fd];

    if (entry->kind != FD_KIND_TTY_IN && entry->kind != FD_KIND_TTY_OUT) {
        return -POSIX_ENOTTY;
    }

    if (request == 0x5301u) {
        if (arg > (uint32_t)TTY_WHITE) return -POSIX_EINVAL;
        tty_set_color((tty_color_t)arg);
        return 0;
    }

    if (request == 0x5302u) {
        int key;

        if (entry->kind != FD_KIND_TTY_IN) return -POSIX_ENOTTY;

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
        if (entry->kind != FD_KIND_TTY_IN) return -POSIX_ENOTTY;
        tty_get_cursor(&g_tty_line_row, &g_tty_line_col);
        tty_text_begin(g_tty_line_row, g_tty_line_col);
        return 0;
    }

    if (request == 0x5304u) {
        const fd_tty_line_t* line = (const fd_tty_line_t*)(uintptr_t)arg;

        if (entry->kind != FD_KIND_TTY_IN || line == 0 || line->buffer == 0) {
            return -POSIX_EINVAL;
        }
        if (line->cursor > line->len) return -POSIX_EINVAL;

        tty_line_redraw(line->buffer, line->len, line->cursor,
                        g_tty_line_row, g_tty_line_col);
        return 0;
    }

    return -POSIX_ENOTTY;
}
