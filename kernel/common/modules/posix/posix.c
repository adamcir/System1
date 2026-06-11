#include "posix.h"
#include "fd_core.h"
#include "fs_core.h"

static int posix_fs_to_errno(int rc) {
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

void posix_init(void) {
    fd_core_init();
}

int posix_open(const char* path, uint32_t flags) {
    return fd_core_open(path, flags);
}

int posix_close(int fd) {
    return fd_core_close(fd);
}

int posix_read(int fd, void* buffer, uint32_t count) {
    return fd_core_read(fd, buffer, count);
}

int posix_write(int fd, const void* buffer, uint32_t count) {
    return fd_core_write(fd, buffer, count);
}

int posix_lseek(int fd, int offset, uint32_t whence) {
    return fd_core_lseek(fd, offset, whence);
}

int posix_stat(const char* path, fs_stat_t* out_stat) {
    int rc = fs_core_stat(path, out_stat);
    if (rc != FS_OK) {
        return posix_fs_to_errno(rc);
    }

    return 0;
}

int posix_fstat(int fd, fs_stat_t* out_stat) {
    return fd_core_fstat(fd, out_stat);
}

int posix_unlink(const char* path) {
    int rc = fs_core_unlink(path);
    if (rc != FS_OK) {
        return posix_fs_to_errno(rc);
    }

    return 0;
}
