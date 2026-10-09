#ifndef SYSTEM_SMOD_H
#define SYSTEM_SMOD_H

#include "types.h"

/* Native System Module container revision 1; no ELF loader is involved. */
#define SMOD_FORMAT_VERSION 1u
#define SMOD_API_VERSION 3u
#define SMOD_ARCH_I386 1u
#define SMOD_ARCH_X86_64 2u
#define SMOD_FLAG_EXECUTABLE 1u
#define SMOD_HEADER_SIZE 32u
#define SMOD_IMAGE_LIMIT 16384u
#define SMOD_BOOT_ARENA_SIZE 65536u

/* Typed interfaces live in core headers; SMOD exports stable ABI tags. */
typedef struct system_display_ops system_display_ops_t;
typedef struct system_input_ops system_input_ops_t;
typedef struct system_irq_chip_ops system_irq_chip_ops_t;
typedef struct system_floppy_controller_ops system_floppy_controller_ops_t;

typedef void (*smod_uart_write_t)(char value);

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} smod_clock_time_t;

typedef int (*smod_rtc_read_t)(smod_clock_time_t* time);

/* SMOD ABI v3: driver callbacks remain inside resident .mod image.
 * Registration is staged and committed only on successful module entry.
 */
typedef struct {
    uint32_t abi_version;
    void (*report_ready)(void);
    int (*register_uart)(smod_uart_write_t write_byte);
    int (*register_rtc)(smod_rtc_read_t read_time);
    int (*register_display)(const system_display_ops_t* ops);
    int (*register_input)(const system_input_ops_t* ops);
    int (*register_irq_chip)(const system_irq_chip_ops_t* ops);
    int (*register_floppy)(const system_floppy_controller_ops_t* ops);
} smod_api_v3_t;

typedef int (*smod_entry_fn_t)(const smod_api_v3_t* api);
#endif
