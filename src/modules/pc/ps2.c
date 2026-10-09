#include "smod.h"
#include "input.h"

/* Native PS/2 keyboard controller, Set 1 scancode decoder and key queue. */
#include "../../kernel/drivers/pc/ps2_core.c"

__attribute__((section(".text.smod_entry")))
int smod_entry(const smod_api_v3_t* api) {
    system_input_ops_t driver;
    if (!api || api->abi_version != SMOD_API_VERSION ||
        !api->register_input) return -1;
    driver.abi = SYSTEM_INPUT_ABI;
    driver.init = ps2_init;
    driver.poll = ps2_poll;
    driver.irq_handler = ps2_irq_handler;
    driver.set_poll_fallback = ps2_set_poll_fallback;
    driver.take_key = ps2_take_key;
    driver.take_char = ps2_take_char;
    driver.last_char = ps2_last_char;
    return api->register_input(&driver);
}
