#include "smod.h"

/* PC-compatible 16550 UART at COM1, 0x3f8. This code runs from .mod. */
#define COM1 0x3f8u

static void uart_out(uint16_t port, uint8_t byte) {
    __asm__ volatile ("outb %0, %w1" : : "a"(byte), "Nd"(port));
}

static uint8_t uart_in(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %w1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void uart_write_byte(char byte) {
    uint32_t tries;
    if (byte == '\n') {
        uart_write_byte('\r');
    }
    for (tries = 0u; tries < 50000u; ++tries) {
        if ((uart_in(COM1 + 5u) & 0x20u) != 0u) {
            uart_out(COM1, (uint8_t)byte);
            return;
        }
    }
}

/* Entry is pinned to image offset zero by the native SMOD linker script. */
__attribute__((section(".text.smod_entry")))
int smod_entry(const smod_api_v2_t* api) {
    uint8_t saved;
    if (!api || api->abi_version != SMOD_API_VERSION || !api->register_uart) return -1;

    /* Scratch-register probe prevents blindly writing to absent UARTs. */
    saved = uart_in(COM1 + 7u);
    uart_out(COM1 + 7u, 0x5au);
    if (uart_in(COM1 + 7u) != 0x5au) {
        uart_out(COM1 + 7u, saved);
        return -1;
    }
    uart_out(COM1 + 7u, saved);

    uart_out(COM1 + 1u, 0u);
    uart_out(COM1 + 3u, 0x80u);  /* DLAB */
    uart_out(COM1 + 0u, 3u);     /* 38400 baud (115200 / 3) */
    uart_out(COM1 + 1u, 0u);
    uart_out(COM1 + 3u, 3u);     /* 8 data bits, no parity, one stop */
    uart_out(COM1 + 2u, 7u);     /* Enable / reset FIFOs */
    uart_out(COM1 + 4u, 3u);     /* DTR + RTS */

    if (api->register_uart(uart_write_byte) != 0) return -1;
    return 0;
}
