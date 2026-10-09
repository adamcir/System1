#include "input.h"
#include "ps2_core.h"

/* PC PS/2 input driver; built in until SMOD loading is implemented. */
static const system_input_ops_t pc_ps2_input_ops = {
    SYSTEM_INPUT_ABI,
    ps2_init,
    ps2_poll,
    ps2_irq_handler,
    ps2_set_poll_fallback,
    ps2_take_key,
    ps2_take_char,
    ps2_last_char
};
void input_platform_init(void) {
    if (!input_has_driver()) (void)input_register(&pc_ps2_input_ops);
}
