#ifndef SYSTEM1_USER_IOCTL_H
#define SYSTEM1_USER_IOCTL_H

#define TIOCSCOLOR     0x5301u
#define TIOCGETKEY      0x5302u
#define TIOCLINEBEGIN   0x5303u
#define TIOCLINEREDRAW  0x5304u
#define TIOCCLEAR       0x5305u

#define TTY_COLOR_BLACK 0u
#define TTY_COLOR_BLUE 1u
#define TTY_COLOR_GREEN 2u
#define TTY_COLOR_CYAN 3u
#define TTY_COLOR_RED 4u
#define TTY_COLOR_MAGENTA 5u
#define TTY_COLOR_BROWN 6u
#define TTY_COLOR_LIGHT_GREY 7u
#define TTY_COLOR_DARK_GREY 8u
#define TTY_COLOR_LIGHT_BLUE 9u
#define TTY_COLOR_LIGHT_GREEN 10u
#define TTY_COLOR_LIGHT_CYAN 11u
#define TTY_COLOR_LIGHT_RED 12u
#define TTY_COLOR_LIGHT_MAGENTA 13u
#define TTY_COLOR_YELLOW 14u
#define TTY_COLOR_WHITE 15u

#define TTY_KEY_NONE         0x00u
#define TTY_KEY_LEFT         0x80u
#define TTY_KEY_RIGHT        0x81u
#define TTY_KEY_INSERT       0x82u
#define TTY_KEY_DELETE       0x83u
#define TTY_KEY_UP           0x84u
#define TTY_KEY_DOWN         0x85u
#define TTY_KEY_CTRL_ALT_DEL 0x86u

struct system1_tty_line {
    const char* buffer;
    unsigned len;
    unsigned cursor;
};

int ioctl(int fd, unsigned request, unsigned arg);

#endif
