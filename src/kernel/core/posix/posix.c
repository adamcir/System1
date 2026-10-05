#include "posix.h"
#include "fd_core.h"
#include "fs_core.h"
#include "process_core.h"
#include "system1_dirent.h"
#include "usermode.h"

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

int posix_symlink(const char* target, const char* linkpath) {
    int rc = fs_core_symlink(target, linkpath);
    return (rc == FS_OK) ? 0 : posix_fs_to_errno(rc);
}

int posix_readlink(const char* path, char* buffer, uint32_t cap) {
    uint32_t size = 0u;
    int rc = fs_core_readlink(path, buffer, cap, &size);

    if (rc != FS_OK) {
        return posix_fs_to_errno(rc);
    }
    return (int)size;
}

int posix_getdents(const char* path, system1_dirent_t* entries, uint32_t cap) {
    fs_dirent_t kernel_entries[16];
    uint32_t count = 0u;
    uint32_t i;
    uint32_t j;
    int rc;

    if (entries == 0 || cap == 0u) {
        return -POSIX_EINVAL;
    }
    if (cap > 16u) {
        cap = 16u;
    }

    rc = fs_core_list_dir(path, kernel_entries, cap, &count);
    if (rc != FS_OK) {
        return posix_fs_to_errno(rc);
    }

    for (i = 0u; i < count; ++i) {
        for (j = 0u; j + 1u < SYSTEM1_DIRENT_NAME_CAP &&
                     kernel_entries[i].name[j] != '\0'; ++j) {
            entries[i].d_name[j] = kernel_entries[i].name[j];
        }
        entries[i].d_name[j] = '\0';
        entries[i].d_type = kernel_entries[i].type;
    }

    return (int)count;
}

int posix_execve(const char* path, char* const argv[], char* const envp[]) {
    if (path == 0) {
        return -POSIX_EINVAL;
    }

    /*
     * The current kernel shell launches a userspace process synchronously.
     * Once a userspace shell exists, execve can replace the calling process
     * without this compatibility return path.
     */
    return usermode_exec(path, argv, envp);
}

void posix_exit(int status) {
    usermode_exit(status);
}
