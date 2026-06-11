#include "syscall_core.h"
#include "fs.h"
#include "posix.h"

void syscall_core_init(void) {
}

int syscall_core_dispatch(uint32_t nr, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a3;

    if (nr == SYS_READ) {
        return posix_read((int)a0, (void*)(uintptr_t)a1, a2);
    }

    if (nr == SYS_WRITE) {
        return posix_write((int)a0, (const void*)(uintptr_t)a1, a2);
    }

    if (nr == SYS_OPEN) {
        return posix_open((const char*)(uintptr_t)a0, a1);
    }

    if (nr == SYS_CLOSE) {
        return posix_close((int)a0);
    }

    if (nr == SYS_LSEEK) {
        return posix_lseek((int)a0, (int)a1, a2);
    }

    if (nr == SYS_STAT) {
        return posix_stat((const char*)(uintptr_t)a0, (fs_stat_t*)(uintptr_t)a1);
    }

    if (nr == SYS_GETCWD) {
        const char* cwd = fs_get_cwd_path();
        char* out = (char*)(uintptr_t)a0;
        uint32_t cap = a1;
        uint32_t i;

        if (out == 0 || cap == 0u) {
            return -POSIX_EINVAL;
        }

        for (i = 0u; i + 1u < cap && cwd[i] != '\0'; ++i) {
            out[i] = cwd[i];
        }

        out[i] = '\0';
        return (int)i;
    }

    if (nr == SYS_CHDIR) {
        int rc = fs_change_dir((const char*)(uintptr_t)a0);
        if (rc != FS_OK) {
            return -fs_to_errno(rc);
        }
        return 0;
    }

    if (nr == SYS_MKDIR) {
        int rc = fs_make_dir((const char*)(uintptr_t)a0);
        if (rc != FS_OK) {
            return -fs_to_errno(rc);
        }
        return 0;
    }

    if (nr == SYS_UNLINK) {
        return posix_unlink((const char*)(uintptr_t)a0);
    }

    return -POSIX_EINVAL;
}
