#include "smod.h"
#include "floppy_controller.h"

/* The same FDC/DMA hardware logic as the original bootstrap driver.
 * Registration is transactional through the System Module ABI.
 */
#define SYSTEM1_SMOD_FDC 1
#include "../../kernel/drivers/pc/floppy_fdc.c"

__attribute__((section(".text.smod_entry")))
int smod_entry(const smod_api_v3_t* api) {
    system_floppy_controller_ops_t driver;
    if (!api || api->abi_version != SMOD_API_VERSION ||
        !api->register_floppy) return -1;
    driver.abi_version = SYSTEM_FLOPPY_ABI;
    driver.read_sector = pc_fdc_read_sector_buffer;
    driver.write_sector = pc_fdc_write_sector_buffer;
    driver.reset_state = pc_fdc_reset_state;
    return api->register_floppy(&driver);
}
