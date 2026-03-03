#include "types.h"
#include "vga_core.h"

#define VGA_WIDTH 80u
#define VGA_HEIGHT 25u
#define VGA_TEXT_PATH_MAX (VGA_WIDTH * VGA_HEIGHT)

static volatile uint16_t* const VGA = (uint16_t*)0xB8000;
static uint16_t row;
static uint16_t col;
static uint8_t color = 7u;
static char text_data[VGA_TEXT_PATH_MAX];
static uint16_t text_path[VGA_TEXT_PATH_MAX + 1u];
static uint16_t text_len;
static uint16_t text_cursor;
static uint8_t text_insert_mode;

static void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void vga_hw_cursor_update(void) {
    uint16_t pos = (uint16_t)(row * VGA_WIDTH + col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFFu));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFFu));
}

static void vga_scroll_up(void) {
    uint16_t r;
    uint16_t c;

    for (r = 1; r < VGA_HEIGHT; ++r) {
        for (c = 0; c < VGA_WIDTH; ++c) {
            VGA[(r - 1u) * VGA_WIDTH + c] = VGA[r * VGA_WIDTH + c];
        }
    }

    for (c = 0; c < VGA_WIDTH; ++c) {
        VGA[(VGA_HEIGHT - 1u) * VGA_WIDTH + c] = ((uint16_t)color << 8) | ' ';
    }
}

static void vga_ensure_row_visible(void) {
    if (row >= VGA_HEIGHT) {
        vga_scroll_up();
        row = (uint16_t)(VGA_HEIGHT - 1u);
    }
}

static uint16_t vga_cursor_pos_get(void) {
    return (uint16_t)(row * VGA_WIDTH + col);
}

static void vga_cursor_pos_set(uint16_t pos) {
    row = (uint16_t)(pos / VGA_WIDTH);
    col = (uint16_t)(pos % VGA_WIDTH);
    vga_hw_cursor_update();
}

static void vga_text_rerender(void) {
    uint16_t pos;
    uint16_t i;

    for (pos = text_path[0]; pos < (VGA_WIDTH * VGA_HEIGHT); ++pos) {
        vga_core_putc_at((uint16_t)(pos / VGA_WIDTH), (uint16_t)(pos % VGA_WIDTH), ' ');
    }

    vga_cursor_pos_set(text_path[0]);
    text_path[0] = vga_cursor_pos_get();

    for (i = 0; i < text_len; ++i) {
        vga_core_putc(text_data[i]);
        text_path[i + 1u] = vga_cursor_pos_get();
    }

    vga_cursor_pos_set(text_path[text_cursor]);
}

void vga_core_init(void) {
    uint16_t i;

    row = 0;
    col = 0;
    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        VGA[i] = ((uint16_t)color << 8) | ' ';
    }
    text_path[0] = 0;
    text_len = 0;
    text_cursor = 0;
    text_insert_mode = 1u;
    vga_hw_cursor_update();
}

void vga_core_set_color(uint8_t new_color) {
    color = (uint8_t)(new_color & 0x0Fu);
}

void vga_core_set_cursor(uint16_t new_row, uint16_t new_col) {
    row = (uint16_t)(new_row % VGA_HEIGHT);
    col = (uint16_t)(new_col % VGA_WIDTH);
    vga_hw_cursor_update();
}

void vga_core_get_cursor(uint16_t* out_row, uint16_t* out_col) {
    if (out_row != 0) {
        *out_row = row;
    }
    if (out_col != 0) {
        *out_col = col;
    }
}

void vga_core_putc_at(uint16_t at_row, uint16_t at_col, char c) {
    uint16_t r = (uint16_t)(at_row % VGA_HEIGHT);
    uint16_t ccol = (uint16_t)(at_col % VGA_WIDTH);
    VGA[r * VGA_WIDTH + ccol] = ((uint16_t)color << 8) | (uint8_t)c;
}

char vga_core_getc_at(uint16_t at_row, uint16_t at_col) {
    uint16_t r = (uint16_t)(at_row % VGA_HEIGHT);
    uint16_t ccol = (uint16_t)(at_col % VGA_WIDTH);
    return (char)(VGA[r * VGA_WIDTH + ccol] & 0xFFu);
}

void vga_core_putc(char c) {
    if (c == '\n') {
        col = 0;
        row++;
        vga_ensure_row_visible();
        vga_hw_cursor_update();
        return;
    }

    VGA[row * VGA_WIDTH + col] = ((uint16_t)color << 8) | (uint8_t)c;
    col++;

    if (col >= VGA_WIDTH) {
        col = 0;
        row++;
    }

    vga_ensure_row_visible();
    vga_hw_cursor_update();
}

void vga_core_puts(const char* s) {
    while (*s) {
        vga_core_putc(*s++);
    }
}

void vga_core_hex_u32(uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";
    int i;

    vga_core_puts("0x");
    for (i = 7; i >= 0; --i) {
        vga_core_putc(hex[(value >> (i * 4)) & 0xFu]);
    }
}

void vga_core_text_begin(uint16_t start_row, uint16_t start_col) {
    vga_core_set_cursor(start_row, start_col);
    text_path[0] = vga_cursor_pos_get();
    text_len = 0;
    text_cursor = 0;
    text_insert_mode = 1u;
    vga_text_rerender();
}

void vga_core_text_putc(char c) {
    uint16_t i;

    if (text_len >= VGA_TEXT_PATH_MAX) {
        return;
    }

    if (text_insert_mode == 0u && text_cursor < text_len) {
        text_data[text_cursor] = c;
        text_cursor++;
        vga_text_rerender();
        return;
    }

    for (i = text_len; i > text_cursor; --i) {
        text_data[i] = text_data[i - 1u];
    }

    text_data[text_cursor] = c;
    text_len++;
    text_cursor++;
    vga_text_rerender();
}

void vga_core_text_backspace(void) {
    uint16_t idx;

    if (text_cursor == 0) {
        return;
    }

    for (idx = text_cursor; idx < text_len; ++idx) {
        text_data[idx - 1u] = text_data[idx];
    }

    text_cursor--;
    text_len--;
    vga_text_rerender();
}

void vga_core_text_left(void) {
    if (text_cursor > 0u) {
        text_cursor--;
        vga_cursor_pos_set(text_path[text_cursor]);
    }
}

void vga_core_text_right(void) {
    if (text_cursor < text_len) {
        text_cursor++;
        vga_cursor_pos_set(text_path[text_cursor]);
    }
}

void vga_core_text_delete(void) {
    uint16_t idx;

    if (text_cursor >= text_len) {
        return;
    }

    for (idx = (uint16_t)(text_cursor + 1u); idx < text_len; ++idx) {
        text_data[idx - 1u] = text_data[idx];
    }

    text_len--;
    vga_text_rerender();
}

void vga_core_text_home(void) {
    uint16_t idx;
    uint16_t row_now;

    idx = text_cursor;
    row_now = (uint16_t)(text_path[idx] / VGA_WIDTH);
    while (idx > 0u) {
        if ((text_path[idx - 1u] / VGA_WIDTH) != row_now) {
            break;
        }
        idx--;
    }

    text_cursor = idx;
    vga_cursor_pos_set(text_path[text_cursor]);
}

void vga_core_text_end(void) {
    uint16_t idx;
    uint16_t row_now;

    idx = text_cursor;
    row_now = (uint16_t)(text_path[idx] / VGA_WIDTH);
    while (idx < text_len) {
        if ((text_path[idx + 1u] / VGA_WIDTH) != row_now) {
            break;
        }
        idx++;
    }

    text_cursor = idx;
    vga_cursor_pos_set(text_path[text_cursor]);
}

void vga_core_text_toggle_insert(void) {
    text_insert_mode = (uint8_t)(text_insert_mode ? 0u : 1u);
}
