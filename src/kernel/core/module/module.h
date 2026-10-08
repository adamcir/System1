#ifndef SYSTEM_MODULE_H
#define SYSTEM_MODULE_H

/* Load compatible modules from the mounted /boot/modules directory.
 * Absent directory and invalid modules do not prevent boot.
 */
int smod_boot_load_all(void);

#endif
