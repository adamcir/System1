#ifndef SYSTEM1_X64_KLOG_H
#define SYSTEM1_X64_KLOG_H

__attribute__((noreturn)) void panic(const char* msg);
void klog_info(const char* prefix, const char* msg);
void klog_system_logo(void);

#endif
