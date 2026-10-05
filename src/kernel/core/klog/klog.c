#include "klog.h"
#include "klog_core.h"

__attribute__((noreturn)) void panic(const char* msg) {
	panic_core(msg);
}

void klog_info(const char* prefix, const char* msg) {
	klog_info_core(prefix, msg);
}

void klog_system_logo(void) {
	klog_system_logo_core();
}

