#include "bootstrap.h"

#include "fs_core.h"
#include "klog.h"
#include "process.h"
#include "syscall.h"

int bootstrap_init(uint32_t boot_magic, uint32_t boot_info_ptr) {
    int rc;

    process_init();
    fs_core_set_boot_context(boot_magic, boot_info_ptr);

    rc = fs_core_init_ramfs();
    if (rc != FS_OK) {
        klog_info("bootstrap", "RAMFS initialization failed");
        return rc;
    }

    klog_info("bootstrap", "RAMFS mounted as root");

    rc = fs_core_mount_boot_media();
    if (rc == FS_OK) {
        klog_info("bootstrap", "Physical filesystem attached");
    } else if (rc == FS_ERR_NOT_FOUND) {
        klog_info("bootstrap", "No physical filesystem; RAM-only mode");
    } else {
        klog_info("bootstrap", "Boot media unavailable; RAM-only mode");
    }

    syscall_init();
    return 0;
}
