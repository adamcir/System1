#ifndef SYSTEM_SMOD_H
#define SYSTEM_SMOD_H

#include "types.h"

/* Native System Module container revision 1; no ELF loader is involved. */
#define SMOD_FORMAT_VERSION 1u
#define SMOD_API_VERSION 2u
#define SMOD_ARCH_I386 1u
#define SMOD_ARCH_X86_64 2u
#define SMOD_FLAG_EXECUTABLE 1u
#define SMOD_HEADER_SIZE 32u
#define SMOD_IMAGE_LIMIT 4096u

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

/* SMOD ABI v2: driver callbacks remain inside resident .mod image.
 * Registration is staged and committed only on successful module entry.
 */
typedef struct {
    uint32_t abi_version;
    void (*report_ready)(void);
    int (*register_uart)(smod_uart_write_t write_byte);
    int (*register_rtc)(smod_rtc_read_t read_time);
} smod_api_v2_t;

typedef int (*smod_entry_fn_t)(const smod_api_v2_t* api);
#endif
