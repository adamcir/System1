#include "bootlog.h"
#include "bootlog_core.h"

void bootlog_info(const char* prefix, const char* msg) {
    bootlog_info_core(prefix, msg);
}
