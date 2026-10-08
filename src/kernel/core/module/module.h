#ifndef SYSTEM_MODULE_H
#define SYSTEM_MODULE_H
#include "types.h"
#include "smod.h"

/* Load compatible modules from the mounted /boot/modules directory.
 * Absent directory and invalid modules do not prevent boot.
 */
int smod_boot_load_all(uint32_t boot_magic);

/* Kernel core consumers; backed by resident hardware SMOD drivers. */
void smod_serial_write(char value);
int smod_rtc_read(smod_clock_time_t* time);

#endif
