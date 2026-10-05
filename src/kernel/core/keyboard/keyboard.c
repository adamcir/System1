#include "types.h"
#include "keyboard.h"
#include "keyboard_core.h"

void keyboard_init(void) {
    keyboard_core_init();
}

void keyboard_set_poll_fallback(uint8_t enabled) {
    keyboard_core_set_poll_fallback(enabled);
}

void keyboard_poll(void) {
    keyboard_core_poll();
}

void keyboard_irq_handler(void) {
    keyboard_core_irq_handler();
}

char keyboard_last_char(void) {
    return keyboard_core_last_char();
}

char keyboard_take_char(void) {
    return keyboard_core_take_char();
}

int keyboard_take_key(void) {
    return keyboard_core_take_key();
}
