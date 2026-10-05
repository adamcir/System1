#ifndef SYSTEM1_COMMON_USERMODE_CORE_H
#define SYSTEM1_COMMON_USERMODE_CORE_H

#include "types.h"

/* Normal short-lived commands. */
#define USERMODE_I386_PROGRAM_MIN       0x00060000u
#define USERMODE_I386_PROGRAM_MAX       0x00070000u
#define USERMODE_I386_STACK_MIN         0x00070000u
#define USERMODE_I386_STACK_TOP         0x00080000u

/* Persistent userspace shell. Kept separate so child commands cannot overwrite it. */
#define USERMODE_I386_SHELL_PROGRAM_MIN 0x00080000u
#define USERMODE_I386_SHELL_PROGRAM_MAX 0x00090000u
#define USERMODE_I386_SHELL_STACK_MIN   0x00090000u
#define USERMODE_I386_SHELL_STACK_TOP   0x0009F000u

#endif
