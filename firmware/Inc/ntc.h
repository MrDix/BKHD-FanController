/**
 * @file ntc.h
 * @brief NTC thermistor temperature measurement via ADC1.
 *
 * Reads the NTC1 voltage divider on PB7 (ADC1_IN11) and converts the ADC
 * sample to a temperature value using the Beta equation.
 *
 * Returns temperature in tenths of degrees Celsius (e.g. 523 = 52.3 C)
 * as a signed int16 so the value fits into the STS frame and can represent
 * under-temperature sensor failures with negative numbers.
 *
 * An invalid/absent sensor returns NTC_TEMP_INVALID.
 */
#ifndef NTC_H
#define NTC_H

#include "main.h"
#include <stdint.h>

#define NTC_TEMP_INVALID  INT16_MIN

void    ntc_init(void);

/* One-shot ADC conversion. Call periodically (e.g. every STATUS_INTERVAL_MS).
 * Non-blocking: returns the last measured value; conversion happens internally
 * with a short polling wait (<1 ms at 12 bit / 160.5 cycles). */
int16_t ntc_read_tenths_celsius(void);

#endif /* NTC_H */
