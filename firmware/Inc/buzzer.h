/**
 * @file buzzer.h
 * @brief Active buzzer control (GPIO on/off + optional blink pattern).
 */
#ifndef BUZZER_H
#define BUZZER_H

#include "main.h"
#include <stdbool.h>

void buzzer_init(void);
void buzzer_on(void);
void buzzer_off(void);
void buzzer_set(bool active);
void buzzer_blink_update(void);  /* call from main loop for pulsed alarm */

#endif /* BUZZER_H */
