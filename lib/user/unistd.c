#include "system1/fcntl.h"
#include "system1/stat.h"
#include "system1/unistd.h"

int open(const char* path, int flags) {
    return system1_syscall(SYS_OPEN, (uint32_t)(uintptr_t)path, (uint32_t)flags, 0u, 0u);
}

int close(int fd) {
    return system1_syscall(SYS_CLOSE, (uint32_t)fd, 0u, 0u, 0u);
}

int read(int fd, void* buf, unsigned count) {
    return system1_syscall(SYS_READ, (uint32_t)fd, (uint32_t)(uintptr_t)buf, (uint32_t)count, 0u);
}

int write(int fd, const void* buf, unsigned count) {
    return system1_syscall(SYS_WRITE, (uint32_t)fd, (uint32_t)(uintptr_t)buf, (uint32_t)count, 0u);
}

int lseek(int fd, int offset, unsigned whence) {
    return system1_syscall(SYS_LSEEK, (uint32_t)fd, (uint32_t)offset, (uint32_t)whence, 0u);
}

int chdir(const char* path) {
    return system1_syscall(SYS_CHDIR, (uint32_t)(uintptr_t)path, 0u, 0u, 0u);
}

int mkdir(const char* path) {
    return system1_syscall(SYS_MKDIR, (uint32_t)(uintptr_t)path, 0u, 0u, 0u);
}

int unlink(const char* path) {
    return system1_syscall(SYS_UNLINK, (uint32_t)(uintptr_t)path, 0u, 0u, 0u);
}

int stat(const char* path, struct stat* out_stat) {
    return system1_syscall(SYS_STAT, (uint32_t)(uintptr_t)path, (uint32_t)(uintptr_t)out_stat, 0u, 0u);
}

char* getcwd(char* buf, unsigned size) {
    int rc = system1_syscall(SYS_GETCWD, (uint32_t)(uintptr_t)buf, (uint32_t)size, 0u, 0u);
    return (rc < 0) ? 0 : buf;
}
