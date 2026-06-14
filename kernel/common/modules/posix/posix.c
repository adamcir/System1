#include "posix.h"
#include "fd_core.h"
#include "fs_core.h"
#include "process_core.h"
#include "sprg.h"

static uint32_t posix_current_arch(void) {
#if defined(__x86_64__)
    return SPRG_ARCH_X86_64;
#else
    return SPRG_ARCH_I386;
#endif
}

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

static int posix_sprg_to_errno(int rc) {
    if (rc == SPRG_ERR_NOT_FOUND) {
        return -POSIX_ENOENT;
    }

    if (rc == SPRG_ERR_BAD_MAGIC ||
        rc == SPRG_ERR_BAD_VERSION ||
        rc == SPRG_ERR_BAD_ARCH ||
        rc == SPRG_ERR_BAD_HEADER ||
        rc == SPRG_ERR_UNSUPPORTED) {
        return -POSIX_ENOEXEC;
    }

    if (rc == SPRG_ERR_TOO_LARGE) {
        return -POSIX_ENOSPC;
    }

    return -POSIX_EINVAL;
}

int posix_execve(const char* path, char* const argv[], char* const envp[]) {
    sprg_image_t image;
    process_t* current;
    int rc;
    (void)argv;
    (void)envp;

    if (path == 0) {
        return -POSIX_EINVAL;
    }

    rc = sprg_validate_file(path, posix_current_arch(), &image);
    if (rc != SPRG_OK) {
        return posix_sprg_to_errno(rc);
    }

    current = process_core_current();
    if (current == 0) {
        return -POSIX_EIO;
    }

    process_core_record_exec_image(current, path, image.arch, image.entry, image.segment_count, image.file_size);
    return -POSIX_ENOSYS;
}
