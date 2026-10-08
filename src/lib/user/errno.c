#include "system1/errno.h"
#include "system1/unistd.h"

int errno;

static unsigned errno_strlen(const char* s) {
    unsigned n = 0u;
    if (s == 0) return 0u;
    while (s[n] != '\0') ++n;
    return n;
}

const char* strerror(int errnum) {
    switch (errnum) {
        case 0: return "Success";
        case EPERM: return "Operation not permitted";
        case ENOENT: return "No such file or directory";
        case ESRCH: return "No such process";
        case EIO: return "Input/output error";
        case E2BIG: return "Argument list too long";
        case ENOEXEC: return "Exec format error";
        case EBADF: return "Bad file descriptor";
        case EACCES: return "Permission denied";
        case EEXIST: return "File exists";
        case ENOTDIR: return "Not a directory";
        case EISDIR: return "Is a directory";
        case EINVAL: return "Invalid argument";
        case ENOTTY: return "Not a tty";
        case ENOSPC: return "No space left on device";
        case EROFS: return "Read-only filesystem";
        case ENOSYS: return "Function not implemented";
        default: return "Unknown error";
    }
}

void perror(const char* prefix) {
    int saved_errno = errno;
    const char* message = strerror(saved_errno);

    if (prefix != 0 && prefix[0] != '\0') {
        (void)write(STDERR_FILENO, prefix, errno_strlen(prefix));
        (void)write(STDERR_FILENO, ": ", 2u);
    }

    (void)write(STDERR_FILENO, message, errno_strlen(message));
    (void)write(STDERR_FILENO, "\n", 1u);
    errno = saved_errno;
}
