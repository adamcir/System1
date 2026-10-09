#ifndef SYSTEM_INPUT_H
#define SYSTEM_INPUT_H
#include "types.h"

#define KEY_NONE 0
#define KEY_LEFT 0x80
#define KEY_RIGHT 0x81
#define KEY_INSERT 0x82
#define KEY_DELETE 0x83
#define KEY_UP 0x84
#define KEY_DOWN 0x85
#define KEY_CTRL_ALT_DEL 0x86
#define SYSTEM_INPUT_ABI 1u

/* Shared hardware-independent input events, used by kernel TTY and POSIX. */
typedef struct system_input_ops {
    uint32_t abi;
    void (*init)(void);
    void (*poll)(void);
    void (*irq_handler)(void);
    void (*set_poll_fallback)(uint8_t);
    int (*take_key)(void);
    char (*take_char)(void);
    char (*last_char)(void);
} system_input_ops_t;

int input_has_driver(void);
int input_register(const system_input_ops_t* ops);
void input_platform_init(void); /* Boot adapter, eventually loaded from SMOD */
void input_init(void);
void input_poll(void);
void input_irq_handler(void);
void input_set_poll_fallback(uint8_t enabled);
int input_take_key(void);
char input_take_char(void);
char input_last_char(void);
#endif
