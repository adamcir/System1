#ifndef SYSTEM_DISPLAY_H
#define SYSTEM_DISPLAY_H
#include "types.h"

/* Platform-neutral console display ABI. TTY owns line editing, not the driver. */
#define SYSTEM_DISPLAY_ABI 1u

typedef struct {
    uint32_t abi;
    void (*init)(void);
    void (*set_color)(uint8_t);
    void (*set_cursor)(uint16_t, uint16_t);
    void (*get_cursor)(uint16_t*, uint16_t*);
    void (*putc)(char);
    void (*puts)(const char*);
    void (*hex_u32)(uint32_t);
    void (*text_begin)(uint16_t, uint16_t);
    void (*text_putc)(char);
    void (*text_backspace)(void);
    void (*text_left)(void);
    void (*text_right)(void);
    void (*text_delete)(void);
    void (*text_toggle_insert)(void);
} system_display_ops_t;

int display_register(const system_display_ops_t* ops);
/* Temporary early-boot adapter; replaced by the SMOD bootstrap loader. */
void display_platform_init(void);
void display_init(void);
void display_set_color(uint8_t color);
void display_get_cursor(uint16_t* row, uint16_t* col);
void display_putc(char ch);
void display_puts(const char* str);
void display_hex_u32(uint32_t value);
void display_text_begin(uint16_t row, uint16_t col);
void display_text_putc(char ch);
void display_text_backspace(void);
void display_text_left(void);
void display_text_right(void);
void display_text_delete(void);
void display_text_toggle_insert(void);
#endif
