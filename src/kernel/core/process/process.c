#include "process.h"
#include "process_core.h"

void process_init(void) {
    process_core_init();
}

process_t* process_current(void) {
    return process_core_current();
}
