#ifndef SYSTEM1_I386_SIGNALS_H
#define SYSTEM1_I386_SIGNALS_H

#define HW_RESET 0
#define HW_PWR_DOWN 1

void signal_raise(int signal);

#endif
