#ifndef SYSTEM_FLOPPY_CONTROLLER_H
#define SYSTEM_FLOPPY_CONTROLLER_H

#include "types.h"

/* Hardware-neutral floppy controller callbacks.
 * Filesystems see only block devices; only the PC driver touches DMA/FDC.
 */
#define SYSTEM_FLOPPY_ABI 1u

typedef struct {
    uint32_t abi_version;
    int (*read_sector)(void* buffer, uint32_t lba);
    int (*write_sector)(const void* buffer, uint32_t lba);
    void (*reset_state)(void);
} system_floppy_controller_ops_t;

int floppy_controller_register(const system_floppy_controller_ops_t* ops);
int floppy_controller_read_sector(void* buffer, uint32_t lba);
int floppy_controller_write_sector(const void* buffer, uint32_t lba);
void floppy_controller_reset(void);

/* Boot-only PC adapter, registered by the i386-floppy platform entry. */
void floppy_controller_platform_init(void);

#endif
