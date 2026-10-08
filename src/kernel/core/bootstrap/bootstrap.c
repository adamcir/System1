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
    (void)smod_boot_load_all();
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
        klog_info("bootstrap", "Userspace shell unavailable; using Kernel Shell");
    } else {
        klog_info("bootstrap", "Userspace shell exited; using Kernel Shell");
    }
    return rc;
}
