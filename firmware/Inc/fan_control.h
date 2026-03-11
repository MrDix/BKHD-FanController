/**
 * @file fan_control.h
 * @brief PWM fan control — set duty cycle per channel.
 */
#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include "main.h"
#include <stdint.h>

void     fan_control_init(void);
void     fan_set_duty(uint8_t channel, uint8_t duty_percent);
uint8_t  fan_get_duty(uint8_t channel);
void     fan_set_all(uint8_t duty_percent);

#endif /* FAN_CONTROL_H */
