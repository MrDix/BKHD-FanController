/**
 * @file tach_measure.h
 * @brief TACH pulse counting via EXTI + RPM calculation.
 */
#ifndef TACH_MEASURE_H
#define TACH_MEASURE_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

void     tach_init(void);
void     tach_update(void);                /* call from main loop */
uint16_t tach_get_rpm(uint8_t channel);
bool     tach_fan_stalled(uint8_t channel); /* true if duty>0 but RPM<threshold */
void     tach_exti_callback(uint16_t pin);  /* called from EXTI ISR */

#endif /* TACH_MEASURE_H */
