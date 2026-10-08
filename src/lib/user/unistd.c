#include "system1/dirent.h"
#include "system1/errno.h"
#include "system1/fcntl.h"
#include "system1/stat.h"
#include "system1/time.h"
#include "system1/unistd.h"

static int posix_result(int rc) {
    if (rc < 0) {
        errno = -rc;
        return -1;
    }
    return rc;
}

int open(const char* path, int flags) {
    return posix_result(system1_syscall(SYS_OPEN,
        (uint32_t)(uintptr_t)path, (uint32_t)flags, 0u, 0u));
}

int close(int fd) {
    return posix_result(system1_syscall(SYS_CLOSE, (uint32_t)fd, 0u, 0u, 0u));
}

int read(int fd, void* buf, unsigned count) {
    return posix_result(system1_syscall(SYS_READ,
        (uint32_t)fd, (uint32_t)(uintptr_t)buf, (uint32_t)count, 0u));
}

int write(int fd, const void* buf, unsigned count) {
    return posix_result(system1_syscall(SYS_WRITE,
        (uint32_t)fd, (uint32_t)(uintptr_t)buf, (uint32_t)count, 0u));
}

int lseek(int fd, int offset, unsigned whence) {
    return posix_result(system1_syscall(SYS_LSEEK,
        (uint32_t)fd, (uint32_t)offset, (uint32_t)whence, 0u));
}

int dup(int oldfd) {
    return posix_result(system1_syscall(SYS_DUP, (uint32_t)oldfd, 0u, 0u, 0u));
}

int dup2(int oldfd, int newfd) {
    return posix_result(system1_syscall(SYS_DUP2,
        (uint32_t)oldfd, (uint32_t)newfd, 0u, 0u));
}

int isatty(int fd) {
    int rc = system1_syscall(SYS_ISATTY, (uint32_t)fd, 0u, 0u, 0u);

    if (rc < 0) {
        errno = -rc;
        return 0;
    }
    if (rc == 0) {
        errno = ENOTTY;
        return 0;
    }
    return 1;
}

int access(const char* path, int mode) {
    return posix_result(system1_syscall(SYS_ACCESS,
        (uint32_t)(uintptr_t)path, (uint32_t)mode, 0u, 0u));
}

int getpid(void) {
    return posix_result(system1_syscall(SYS_GETPID, 0u, 0u, 0u, 0u));
}

int getppid(void) {
    return posix_result(system1_syscall(SYS_GETPPID, 0u, 0u, 0u, 0u));
}

int chdir(const char* path) {
    return posix_result(system1_syscall(SYS_CHDIR,
        (uint32_t)(uintptr_t)path, 0u, 0u, 0u));
}

int mkdir(const char* path) {
    return posix_result(system1_syscall(SYS_MKDIR,
        (uint32_t)(uintptr_t)path, 0u, 0u, 0u));
}

int unlink(const char* path) {
    return posix_result(system1_syscall(SYS_UNLINK,
        (uint32_t)(uintptr_t)path, 0u, 0u, 0u));
}

int symlink(const char* target, const char* linkpath) {
    return posix_result(system1_syscall(SYS_SYMLINK,
        (uint32_t)(uintptr_t)target,
        (uint32_t)(uintptr_t)linkpath, 0u, 0u));
}

int readlink(const char* path, char* buf, unsigned size) {
    return posix_result(system1_syscall(SYS_READLINK,
        (uint32_t)(uintptr_t)path,
        (uint32_t)(uintptr_t)buf,
        (uint32_t)size, 0u));
}

int getdents(const char* path, struct dirent* entries, unsigned cap) {
    return posix_result(system1_syscall(SYS_GETDENTS,
        (uint32_t)(uintptr_t)path,
        (uint32_t)(uintptr_t)entries,
        (uint32_t)cap, 0u));
}

int execve(const char* path, char* const argv[], char* const envp[]) {
    return posix_result(system1_syscall(SYS_EXECVE,
        (uint32_t)(uintptr_t)path,
        (uint32_t)(uintptr_t)argv,
        (uint32_t)(uintptr_t)envp, 0u));
}

int stat(const char* path, struct stat* out_stat) {
    return posix_result(system1_syscall(SYS_STAT,
        (uint32_t)(uintptr_t)path,
        (uint32_t)(uintptr_t)out_stat, 0u, 0u));
}

int fstat(int fd, struct stat* out_stat) {
    return posix_result(system1_syscall(SYS_FSTAT,
        (uint32_t)fd, (uint32_t)(uintptr_t)out_stat, 0u, 0u));
}

char* getcwd(char* buf, unsigned size) {
    int rc = system1_syscall(SYS_GETCWD,
        (uint32_t)(uintptr_t)buf, (uint32_t)size, 0u, 0u);

    if (rc < 0) {
        errno = -rc;
        return 0;
    }
    return buf;
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
    return posix_result(system1_syscall(SYS_NANOSLEEP,
        (uint32_t)(uintptr_t)req,
        (uint32_t)(uintptr_t)rem, 0u, 0u));
}

unsigned sleep(unsigned seconds) {
    struct timespec req;
    req.tv_sec = (int32_t)seconds;
    req.tv_nsec = 0;
    return (nanosleep(&req, 0) == 0) ? 0u : seconds;
}

int usleep(unsigned usec) {
    struct timespec req;

    req.tv_sec = (int32_t)(usec / 1000000u);
    req.tv_nsec = (int32_t)((usec % 1000000u) * 1000u);
    return nanosleep(&req, 0);
}

void _exit(int status) {
    (void)system1_syscall(SYS_EXIT, (uint32_t)status, 0u, 0u, 0u);
    for (;;) {
    }
}

int ioctl(int fd, unsigned request, unsigned arg) {
    return posix_result(system1_syscall(SYS_IOCTL,
        (uint32_t)fd, (uint32_t)request, (uint32_t)arg, 0u));
}

int reboot(unsigned how) {
    return posix_result(system1_syscall(SYS_REBOOT,
        (uint32_t)how, 0u, 0u, 0u));
}
