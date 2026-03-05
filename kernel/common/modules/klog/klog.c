#include "klog.h"
#include "klog_core.h"

void klog_info(const char* prefix, const char* msg) {
    klog_info_core(prefix, msg);
}

void klog_system_logo(void) {
	klog_system_logo_core();
}

