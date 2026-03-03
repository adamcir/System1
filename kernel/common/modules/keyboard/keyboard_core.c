#include "types.h"
#include "keyboard_core.h"

#define KEY_BUFFER_SIZE 64u

static volatile int key_buffer[KEY_BUFFER_SIZE];
static volatile uint8_t key_head;
static volatile uint8_t key_tail;

static uint8_t shift_pressed;
static uint8_t extended_prefix;
static uint8_t poll_fallback_enabled;

static uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void queue_push(int key) {
    uint8_t next = (uint8_t)((key_head + 1u) & (KEY_BUFFER_SIZE - 1u));
    if (next == key_tail) {
        return;
    }

    key_buffer[key_head] = key;
    key_head = next;
}

static int queue_pop(void) {
    int key;

    if (key_head == key_tail) {
        return KEY_NONE;
    }

    key = key_buffer[key_tail];
    key_tail = (uint8_t)((key_tail + 1u) & (KEY_BUFFER_SIZE - 1u));
    return key;
}

static int queue_peek(void) {
    if (key_head == key_tail) {
        return KEY_NONE;
    }

    return key_buffer[key_tail];
}

static char translate_scancode(uint8_t scancode) {
    static const char base_map[128] = {
        [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
        [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
        [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
        [0x0E] = '\b',
        [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
        [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
        [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
        [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
        [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
        [0x26] = 'l', [0x27] = ';', [0x28] = '\'',
        [0x29] = '`', [0x2B] = '\\',
        [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
        [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',',
        [0x34] = '.', [0x35] = '/', [0x39] = ' ',
        [0x1C] = '\n'
    };

    static const char shift_map[128] = {
        [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
        [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
        [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
        [0x0E] = '\b',
        [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
        [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
        [0x18] = 'O', [0x19] = 'P', [0x1A] = '{', [0x1B] = '}',
        [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F',
        [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
        [0x26] = 'L', [0x27] = ':', [0x28] = '"',
        [0x29] = '~', [0x2B] = '|',
        [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V',
        [0x30] = 'B', [0x31] = 'N', [0x32] = 'M', [0x33] = '<',
        [0x34] = '>', [0x35] = '?', [0x39] = ' ',
        [0x1C] = '\n'
    };

    if (scancode >= 128) {
        return 0;
    }

    return shift_pressed ? shift_map[scancode] : base_map[scancode];
}

static void keyboard_process_scancode(uint8_t scancode) {
    char translated;

    if (scancode == 0xE0) {
        extended_prefix = 1;
        return;
    }

    if (extended_prefix) {
        if ((scancode & 0x80u) == 0) {
            if (scancode == 0x4B) {
                queue_push(KEY_LEFT);
            } else if (scancode == 0x4D) {
                queue_push(KEY_RIGHT);
            } else if (scancode == 0x52) {
                queue_push(KEY_INSERT);
            } else if (scancode == 0x53) {
                queue_push(KEY_DELETE);
            } else if (scancode == 0x47) {
                queue_push(KEY_HOME);
            } else if (scancode == 0x4F) {
                queue_push(KEY_END);
            }
        }
        extended_prefix = 0;
        return;
    }

    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }

    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return;
    }

    if (scancode & 0x80u) {
        return;
    }

    translated = translate_scancode(scancode);
    if (translated != 0) {
        queue_push((int)translated);
    }
}

void keyboard_core_init(void) {
    key_head = 0;
    key_tail = 0;
    shift_pressed = 0;
    extended_prefix = 0;
    poll_fallback_enabled = 0;
}

void keyboard_core_set_poll_fallback(uint8_t enabled) {
    poll_fallback_enabled = enabled ? 1u : 0u;
}

void keyboard_core_poll(void) {
    uint8_t status;

    if (poll_fallback_enabled == 0) {
        return;
    }

    status = inb(0x64);
    if ((status & 0x01u) == 0) {
        return;
    }

    keyboard_process_scancode(inb(0x60));
}

void keyboard_core_irq_handler(void) {
    keyboard_process_scancode(inb(0x60));
}

char keyboard_core_last_char(void) {
    int key = queue_peek();
    if (key > 0 && key < 128) {
        return (char)key;
    }
    return 0;
}

char keyboard_core_take_char(void) {
    int key = queue_pop();
    if (key > 0 && key < 128) {
        return (char)key;
    }
    return 0;
}

int keyboard_core_take_key(void) {
    return queue_pop();
}
