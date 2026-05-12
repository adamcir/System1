#ifndef SYSTEM1_COMMON_KLOG_CORE_H
#define SYSTEM1_COMMON_KLOG_CORE_H

__attribute__((noreturn)) void panic_core(const char* msg);
void klog_info_core(const char* prefix, const char* msg);
void klog_system_logo_core(void);

#endif
