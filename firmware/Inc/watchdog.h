/**
 * @file watchdog.h
 * @brief Host communication watchdog (60s timeout) and HW IWDG.
 */
#ifndef WATCHDOG_H
#define WATCHDOG_H

#include "main.h"
#include <stdbool.h>

void watchdog_init(void);
void watchdog_host_feed(void);         /* call on every valid host command */
void watchdog_check(void);             /* call from main loop */
bool watchdog_is_failsafe(void);       /* true if host timeout occurred */
void watchdog_clear_failsafe(void);

#endif /* WATCHDOG_H */
