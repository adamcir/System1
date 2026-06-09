#include "posix.h"
#include "fd_core.h"

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
