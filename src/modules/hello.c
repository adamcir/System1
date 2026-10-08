#include "smod.h"

/* Position-independent and self-contained: no external references or GOT.
 * The image is packed from the .text section by tools/mksmod.py.
 */
int smod_entry(const smod_api_v1_t* api) {
    if (api == 0 || api->abi_version != SMOD_API_VERSION ||
        api->report_ready == 0) return -1;
    api->report_ready();
    return 0;
}
