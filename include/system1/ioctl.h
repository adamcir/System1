#ifndef SYSTEM1_USER_IOCTL_H
#define SYSTEM1_USER_IOCTL_H

#define TIOCSCOLOR 0x5301u

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

int ioctl(int fd, unsigned request, unsigned arg);

#endif
