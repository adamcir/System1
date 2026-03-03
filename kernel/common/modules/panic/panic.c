#include "panic.h"
#include "panic_core.h"

__attribute__((noreturn)) void panic(const char* msg) {
    panic_core(msg);
}
