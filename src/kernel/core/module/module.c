#include "module.h"
#include "module_core.h"

int smod_boot_load_all(uint32_t boot_magic) {
    return smod_core_boot_load_all(boot_magic);
}
