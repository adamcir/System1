#ifndef SYSTEM1_USER_REBOOT_H
#define SYSTEM1_USER_REBOOT_H

#define RB_AUTOBOOT  0u
#define RB_POWER_OFF 1u

int reboot(unsigned how);

#endif
