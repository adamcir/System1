#include "bootlog.h"
#include "bootlog_core.h"

#if defined(SYSTEM1_BOOTLOG_I386)
#define BOOTLOG_PREFIX_VALUE "[boot - i386] "
#elif defined(SYSTEM1_BOOTLOG_X86_64)
#define BOOTLOG_PREFIX_VALUE "[boot - x86_64] "
#elif defined(SYSTEM1_BOOTLOG_I386_FLOPPY)
#define BOOTLOG_PREFIX_VALUE "[boot - i386-floppy] "
#else
#define BOOTLOG_PREFIX_VALUE "[boot] "
#endif

void bootlog_info(const char* msg) {
    bootlog_info_core(BOOTLOG_PREFIX_VALUE, msg);
}
