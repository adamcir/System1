#ifndef SYSTEM1_USER_TIME_H
#define SYSTEM1_USER_TIME_H

#include "types.h"

struct timespec {
    int32_t tv_sec;
    int32_t tv_nsec;
};

int nanosleep(const struct timespec* req, struct timespec* rem);

#endif
