#include "devfs.h"
#include "tty.h"
#include "module.h"
#include "klog.h"

/* Character devices use VFS descriptors; nothing persists to the disk. */
int devfs_read(uint32_t which, uint32_t offset, char* out, uint32_t cap, uint32_t* read) {
    uint32_t i;
    (void)offset;
    if (!out || !read) return FS_ERR_INVALID;
    *read = 0u;
    if (which == 0u) return FS_OK;             /* /dev/null */
    if (which == 1u) {                         /* /dev/zero */
        for (i = 0u; i < cap; ++i) out[i] = 0;
        *read = cap;
        return FS_OK;
    }
    if (which == 2u || which == 3u) {         /* /dev/tty, /dev/console */
        int rc = tty_readline(out, cap);
        if (rc < 0) return FS_ERR_INVALID;
        *read = (uint32_t)rc;
        return FS_OK;
    }
    if (which == 4u || which == 5u) return FS_OK; /* serial TX / kernel log RX unsupported */
    return FS_ERR_NOT_FOUND;
}

int devfs_write(uint32_t which, const char* data, uint32_t len, uint32_t* written) {
    uint32_t i;
    if (!written || (len != 0u && !data)) return FS_ERR_INVALID;
    *written = 0u;
    if (which == 0u || which == 1u) {
        *written = len;                        /* /dev/null, /dev/zero */
        return FS_OK;
    }
    if (which == 2u || which == 3u) {
        for (i = 0u; i < len; ++i) tty_putc(data[i]);
        *written = len;
        return FS_OK;
    }
    if (which == 4u) { /* /dev/ttyS0: COM1 transmitter from SMOD */
        for (i = 0u; i < len; ++i) smod_serial_write(data[i]);
        *written = len;
        return FS_OK;
    }
    if (which == 5u) { /* /dev/kmsg: user messages go to kernel logger */
        char line[96];
        uint32_t off = 0u;
        while (off < len) {
            uint32_t n = 0u;
            while (off < len && n + 1u < sizeof(line)) {
                char ch = data[off++];
                if (ch == '\n' || ch == '\r') break;
                line[n++] = ch;
            }
            line[n] = '\0';
            if (n != 0u) klog_info("kmsg", line);
        }
        *written = len;
        return FS_OK;
    }
    return FS_ERR_NOT_FOUND;
}
