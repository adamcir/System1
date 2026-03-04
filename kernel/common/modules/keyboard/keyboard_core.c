#include "types.h"
#include "keyboard_core.h"

#define KEY_BUFFER_SIZE 64u
#define KBD_DATA_PORT 0x60u
#define KBD_STATUS_PORT 0x64u
#define KBD_STATUS_IBF 0x02u
#define KBD_STATUS_OBF 0x01u
#define KBD_CMD_SET_LEDS 0xEDu
#define KBD_ACK 0xFAu
#define KBD_RESEND 0xFEu
#define KBD_IO_TIMEOUT 65535u

static volatile int key_buffer[KEY_BUFFER_SIZE];
static volatile uint8_t key_head;
static volatile uint8_t key_tail;

static uint8_t shift_pressed;
static uint8_t ctrl_pressed;
static uint8_t alt_pressed;
static uint8_t capslock_on;
static uint8_t numlock_on;
static uint8_t extended_prefix;
static uint8_t poll_fallback_enabled;

static uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t keyboard_wait_input_clear(void) {
    uint32_t i;

    for (i = 0; i < KBD_IO_TIMEOUT; ++i) {
        if ((inb(KBD_STATUS_PORT) & KBD_STATUS_IBF) == 0u) {
            return 1u;
        }
    }
    return 0u;
}

static uint8_t keyboard_wait_output_full(void) {
    uint32_t i;

    for (i = 0; i < KBD_IO_TIMEOUT; ++i) {
        if ((inb(KBD_STATUS_PORT) & KBD_STATUS_OBF) != 0u) {
            return 1u;
        }
    }
    return 0u;
}

static uint8_t keyboard_send_led_mask(uint8_t leds) {
    uint8_t ack;

    if (keyboard_wait_input_clear() == 0u) {
        return 0u;
    }
    outb(KBD_DATA_PORT, KBD_CMD_SET_LEDS);

    if (keyboard_wait_output_full() == 0u) {
        return 0u;
    }
    ack = inb(KBD_DATA_PORT);
    if (ack != KBD_ACK) {
        return 0u;
    }

    if (keyboard_wait_input_clear() == 0u) {
        return 0u;
    }
    outb(KBD_DATA_PORT, leds);

    if (keyboard_wait_output_full() == 0u) {
        return 0u;
    }
    ack = inb(KBD_DATA_PORT);
    return (uint8_t)(ack == KBD_ACK);
}

static void keyboard_sync_leds(void) {
    uint8_t leds = 0u;
    uint8_t attempt;

    if (numlock_on) {
        leds |= 0x02u;
    }
    if (capslock_on) {
        leds |= 0x04u;
    }

    for (attempt = 0u; attempt < 2u; ++attempt) {
        if (keyboard_send_led_mask(leds)) {
            return;
        }
    }
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

static uint8_t is_ascii_alpha(char c) {
    return (uint8_t)((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

static char ascii_to_upper(char c) {
    if (c >= 'a' && c <= 'z') {
        return (char)(c - ('a' - 'A'));
    }
    return c;
}

static char ascii_to_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

static char translate_scancode(uint8_t scancode) {
    static const char base_map[128] = {
        [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
        [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
        [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
        [0x0E] = '\b',
        [0x0F] = '\t',
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
        [0x0F] = '\t',
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

    char translated;
    uint8_t use_upper;

    if (scancode >= 128) {
        return 0;
    }
    if (shift_pressed) {
        translated = shift_map[scancode];
    } else {
        translated = base_map[scancode];
    }
    if (translated == 0 || is_ascii_alpha(translated) == 0u) {
        return translated;
    }

    use_upper = (uint8_t)((shift_pressed ? 1u : 0u) ^ (capslock_on ? 1u : 0u));
    return use_upper ? ascii_to_upper(translated) : ascii_to_lower(translated);
}

static void keyboard_process_scancode(uint8_t scancode) {
    char translated;
    uint8_t is_break;
    uint8_t code;

    if (scancode == 0xE0) {
        extended_prefix = 1;
        return;
    }

    if (extended_prefix) {
        is_break = (uint8_t)(scancode & 0x80u);
        code = (uint8_t)(scancode & 0x7Fu);

        if (code == 0x1D) {
            ctrl_pressed = is_break ? 0u : 1u;
            extended_prefix = 0;
            return;
        }

        if (code == 0x38) {
            alt_pressed = is_break ? 0u : 1u;
            extended_prefix = 0;
            return;
        }

        if (is_break == 0) {
            if (code == 0x4B) {
                queue_push(KEY_LEFT);
            } else if (code == 0x4D) {
                queue_push(KEY_RIGHT);
            } else if (code == 0x52) {
                queue_push(KEY_INSERT);
            } else if (code == 0x53) {
                if (ctrl_pressed && alt_pressed) {
                    queue_push(KEY_CTRL_ALT_DEL);
                } else {
                    queue_push(KEY_DELETE);
                }
            } else if (code == 0x1C) {
                queue_push('\n');
            } else if (code == 0x35) {
                queue_push('/');
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

    if (scancode == 0x1D) {
        ctrl_pressed = 1;
        return;
    }

    if (scancode == 0x9D) {
        ctrl_pressed = 0;
        return;
    }

    if (scancode == 0x38) {
        alt_pressed = 1;
        return;
    }

    if (scancode == 0xB8) {
        alt_pressed = 0;
        return;
    }

    if (scancode == 0x3A) {
        if ((scancode & 0x80u) == 0u) {
            capslock_on = (uint8_t)(capslock_on ? 0u : 1u);
            keyboard_sync_leds();
        }
        return;
    }

    if (scancode == 0x45) {
        if ((scancode & 0x80u) == 0u) {
            numlock_on = (uint8_t)(numlock_on ? 0u : 1u);
            keyboard_sync_leds();
        }
        return;
    }

    if ((scancode & 0x80u) != 0u) {
        return;
    }

    if (scancode >= 0x47u && scancode <= 0x53u) {
        uint8_t number_mode = (uint8_t)((numlock_on ? 1u : 0u) ^ (shift_pressed ? 1u : 0u));
        switch (scancode) {
            case 0x47: if (number_mode) { queue_push('7'); } return;
            case 0x48: if (number_mode) { queue_push('8'); } return;
            case 0x49: if (number_mode) { queue_push('9'); } return;
            case 0x4A: queue_push('-'); return;
            case 0x4B: queue_push(number_mode ? '4' : KEY_LEFT); return;
            case 0x4C: if (number_mode) { queue_push('5'); } return;
            case 0x4D: queue_push(number_mode ? '6' : KEY_RIGHT); return;
            case 0x4E: queue_push('+'); return;
            case 0x4F: if (number_mode) { queue_push('1'); } return;
            case 0x50: if (number_mode) { queue_push('2'); } return;
            case 0x51: if (number_mode) { queue_push('3'); } return;
            case 0x52: queue_push(number_mode ? '0' : KEY_INSERT); return;
            case 0x53: queue_push('.'); return;
            default: break;
        }
    }

    if (scancode == 0x37) {
        queue_push('*');
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
    ctrl_pressed = 0;
    alt_pressed = 0;
    capslock_on = 0;
    numlock_on = 0u;
    extended_prefix = 0;
    poll_fallback_enabled = 0;
    keyboard_sync_leds();
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
