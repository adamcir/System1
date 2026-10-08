#include "floppy_controller.h"
#include "fs_core.h"

/* Common sector-device adapter. No I/O ports or PC-specific DMA here. */
static const system_floppy_controller_ops_t* controller;

int floppy_controller_register(const system_floppy_controller_ops_t* ops) {
    if (!ops || ops->abi_version != SYSTEM_FLOPPY_ABI ||
        !ops->read_sector || !ops->write_sector || !ops->reset_state) {
        return FS_ERR_INVALID;
    }
    controller = ops;
    return FS_OK;
}

int floppy_controller_read_sector(void* buffer, uint32_t lba) {
    if (!controller || !buffer || lba >= 2880u) return FS_ERR_INVALID;
    return controller->read_sector(buffer, lba);
}

int floppy_controller_write_sector(const void* buffer, uint32_t lba) {
    if (!controller || !buffer || lba >= 2880u) return FS_ERR_INVALID;
    return controller->write_sector(buffer, lba);
}

void floppy_controller_reset(void) {
    if (controller) controller->reset_state();
}
