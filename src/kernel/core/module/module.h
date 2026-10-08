#ifndef SYSTEM_MODULE_H
#define SYSTEM_MODULE_H
#include "types.h"

/* Load compatible modules from the mounted /boot/modules directory.
 * Absent directory and invalid modules do not prevent boot.
 */
int smod_boot_load_all(uint32_t boot_magic);

#endif
