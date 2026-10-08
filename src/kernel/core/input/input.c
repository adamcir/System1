#include "input.h"

static const system_input_ops_t* active_input;

int input_register(const system_input_ops_t* ops) {
    if (!ops || ops->abi != SYSTEM_INPUT_ABI ||
        !ops->init || !ops->poll || !ops->irq_handler ||
        !ops->set_poll_fallback || !ops->take_key ||
        !ops->take_char || !ops->last_char) {
        return -1;
    }
    active_input = ops;
    return 0;
}
void input_init(void) { if (active_input) active_input->init(); }
void input_poll(void) { if (active_input) active_input->poll(); }
void input_irq_handler(void) { if (active_input) active_input->irq_handler(); }
void input_set_poll_fallback(uint8_t v) {
    if (active_input) active_input->set_poll_fallback(v);
}
int input_take_key(void) {
    return active_input ? active_input->take_key() : KEY_NONE;
}
char input_take_char(void) {
    return active_input ? active_input->take_char() : 0;
}
char input_last_char(void) {
    return active_input ? active_input->last_char() : 0;
}
