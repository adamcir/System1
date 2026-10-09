#include "bootstrap.h"

#include "fs_core.h"
#include "klog.h"
#include "module.h"
#include "posix.h"
#include "process.h"
#include "syscall.h"
#include "tty.h"

int bootstrap_init(uint32_t boot_magic, uint32_t boot_info_ptr) {
    int rc;

    process_init();
    klog_info("system", "Copyright (c) 2026 Adam Cir (Adava), Adava Software, Adava Development.");
    fs_core_set_boot_context(boot_magic, boot_info_ptr);

    rc = fs_core_init_ramfs();
    if (rc != FS_OK) {
        klog_info("bootstrap", "RAMFS initialization failed");
        return rc;
    }

    klog_info("bootstrap", "RAMFS mounted as root");

    rc = fs_core_mount_boot_media();
    if (rc == FS_OK) {
        klog_info("bootstrap", "Physical filesystem mounted as root");
    } else if (rc == FS_ERR_NOT_FOUND) {
        klog_info("bootstrap", "No physical filesystem; RAM-only mode");
    } else {
        klog_info("bootstrap", "Boot media unavailable; RAM-only mode");
    }

    syscall_init();
    (void)smod_boot_load_all(boot_magic);

    /* VFS smoke test: devfs and procfs are live virtual mounts, not files
     * copied to FAT12/ISO9660. Must work with RAMFS and physical roots.
     */
    {
        fs_stat_t dev, proc;
        uint32_t id, got = 0u, wrote = 0u;
        char zeros[4];
        char info[48];
        if (fs_core_stat("/dev", &dev) == FS_OK &&
            fs_core_stat("/proc", &proc) == FS_OK &&
            (dev.mode & FS_MODE_DIR) != 0u &&
            (proc.mode & FS_MODE_DIR) != 0u &&
            fs_core_open("/dev/zero", FS_O_RDONLY, &id) == FS_OK &&
            fs_core_read(id, 0u, zeros, sizeof(zeros), &got) == FS_OK &&
            got == sizeof(zeros) &&
            zeros[0] == 0 && zeros[1] == 0 &&
            zeros[2] == 0 && zeros[3] == 0 &&
            fs_core_open("/proc/version", FS_O_RDONLY, &id) == FS_OK &&
            fs_core_read(id, 0u, info, sizeof(info), &got) == FS_OK &&
            got > 0u &&
            fs_core_open("/dev/null", FS_O_WRONLY, &id) == FS_OK &&
            fs_core_write(id, 0u, "ok", 2u, &wrote) == FS_OK &&
            wrote == 2u &&
            fs_core_open("/dev/tty", FS_O_WRONLY, &id) == FS_OK &&
            fs_core_write(id, 0u, "", 0u, &wrote) == FS_OK &&
            wrote == 0u &&
            fs_core_open("/dev/kmsg", FS_O_WRONLY, &id) == FS_OK &&
            fs_core_write(id, 0u, "devfs write verified", 20u, &wrote) == FS_OK &&
            wrote == 20u) {
            klog_info("vfs", "/dev and /proc live");
        } else {
            klog_info("vfs", "Pseudo filesystem check failed");
        }
    }
    return 0;
}


static void bootstrap_copy_string(char* dst, uint32_t cap, const char* src) {
    uint32_t i = 0u;
    if (dst == 0 || cap == 0u) return;
    while (src != 0 && src[i] != '\0' && i + 1u < cap) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static void bootstrap_load_shell_path(char* out, uint32_t cap) {
    char cfg[256];
    uint32_t size = 0u;
    uint32_t i = 0u;
    const char* fallback = "/bin/sh.prg";

    bootstrap_copy_string(out, cap, fallback);

    if (fs_core_read_file("/etc/kernel.cfg", cfg, sizeof(cfg) - 1u, &size) != FS_OK) {
        return;
    }
    cfg[size] = '\0';

    while (i < size) {
        uint32_t start = i;
        uint32_t end;
        while (i < size && cfg[i] != '\n' && cfg[i] != '\r') ++i;
        end = i;
        while (i < size && (cfg[i] == '\n' || cfg[i] == '\r')) ++i;

        if (end > start + 6u &&
            cfg[start] == 's' && cfg[start + 1u] == 'h' &&
            cfg[start + 2u] == 'e' && cfg[start + 3u] == 'l' &&
            cfg[start + 4u] == 'l' && cfg[start + 5u] == '=') {
            uint32_t p = 0u;
            uint32_t j = start + 6u;
            while (j < end && p + 1u < cap) out[p++] = cfg[j++];
            out[p] = '\0';
            if (p == 0u) bootstrap_copy_string(out, cap, fallback);
            return;
        }
    }
}

int bootstrap_start_shell(void) {
    char shell_path[FS_PATH_CAP];
    char* argv[2];
    int rc;

    bootstrap_load_shell_path(shell_path, FS_PATH_CAP);
    argv[0] = shell_path;
    argv[1] = 0;

    klog_info("bootstrap", "Starting configured userspace shell");
    /* Boot diagnostics use colors; normal userspace starts with white text. */
    tty_set_color(TTY_WHITE);
    rc = posix_execve(shell_path, argv, 0);
    if (rc < 0) {
        tty_puts("bootstrap: shell exec failure errno=");
        tty_hex_u32((uint32_t)(-rc));
        tty_putc('\n');
    }
    if (rc < 0) {
        klog_info("bootstrap", "Userspace shell unavailable; using Kernel Shell");
    } else {
        klog_info("bootstrap", "Userspace shell exited; using Kernel Shell");
    }
    return rc;
}
