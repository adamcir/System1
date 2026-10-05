#include "bootstrap.h"
#include "fs.h"
#include "klog.h"
#include "process.h"
#include "syscall.h"

int bootstrap_init(uint32_t boot_magic, uint32_t boot_info_ptr) {
    fs_core_stats_t fs_stats;

    process_init();
    fs_set_boot_context(boot_magic, boot_info_ptr);

    if (fs_init() != FS_OK) {
        return -1;
    }

    fs_get_stats(&fs_stats);
    klog_info("bootstrap", "RAMFS mounted as root");

    if (fs_stats.boot_media_kind == 0u) {
        klog_info("bootstrap", "No physical filesystem; RAM-only mode");
    } else {
        klog_info("bootstrap", "Physical filesystem attached");
    }

    syscall_init();
    return 0;
}
