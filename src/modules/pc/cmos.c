#include "smod.h"

/* PC AT CMOS real-time clock. Driver executes from /boot/modules/cmos.mod. */
static void cmos_out(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %w1" : : "a"(val), "Nd"(port));
}
static uint8_t cmos_in(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %w1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static uint8_t cmos_reg(uint8_t index) {
    cmos_out(0x70u, (uint8_t)(index | 0x80u));
    return cmos_in(0x71u);
}
static uint8_t cmos_decimal(uint8_t n) {
    return (uint8_t)((n & 0x0fu) + ((n >> 4) * 10u));
}

static int cmos_read_clock(smod_clock_time_t* time) {
    uint32_t tries;
    uint8_t sec, min, hour, day, month, year, century, status;
    if (!time) return -1;

    /* Wait boundedly for the RTC update-in-progress bit to clear. */
    for (tries = 0u; tries < 100000u; ++tries) {
        if ((cmos_reg(0x0au) & 0x80u) == 0u) break;
    }
    if (tries == 100000u) return -1;

    sec = cmos_reg(0u);
    min = cmos_reg(2u);
    hour = cmos_reg(4u);
    day = cmos_reg(7u);
    month = cmos_reg(8u);
    year = cmos_reg(9u);
    century = cmos_reg(0x32u);
    status = cmos_reg(0x0bu);
    if ((cmos_reg(0x0au) & 0x80u) != 0u) return -1;

    /* Convert both BCD and binary RTC modes, including 12-hour AM/PM. */
    if (!(status & 4u)) {
        sec = cmos_decimal(sec);
        min = cmos_decimal(min);
        hour = (uint8_t)((hour & 0x80u) | cmos_decimal((uint8_t)(hour & 0x7fu)));
        day = cmos_decimal(day);
        month = cmos_decimal(month);
        year = cmos_decimal(year);
        century = cmos_decimal(century);
    }
    if (!(status & 2u)) {
        uint8_t pm = (uint8_t)(hour & 0x80u);
        hour = (uint8_t)(hour & 0x7fu);
        if (hour == 12u) hour = 0u;
        if (pm) hour = (uint8_t)(hour + 12u);
    }
    if (sec > 59u || min > 59u || hour > 23u ||
        day == 0u || day > 31u || month == 0u || month > 12u) return -1;

    time->year = (uint16_t)(((century >= 19u && century <= 21u) ? century * 100u : 2000u) + year);
    time->month = month;
    time->day = day;
    time->hour = hour;
    time->minute = min;
    time->second = sec;
    return 0;
}

__attribute__((section(".text.smod_entry")))
int smod_entry(const smod_api_v2_t* api) {
    smod_clock_time_t now;
    if (!api || api->abi_version != SMOD_API_VERSION || !api->register_rtc) return -1;
    if (cmos_read_clock(&now) != 0) return -1;
    return api->register_rtc(cmos_read_clock);
}
