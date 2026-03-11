/**
 * @file fan_control.c
 * @brief PWM duty cycle control for 5 fan channels.
 *
 * The PCB uses NPN open-collector drivers that INVERT the MCU output:
 *   MCU HIGH -> NPN ON  -> Fan PWM LOW
 *   MCU LOW  -> NPN OFF -> Fan PWM HIGH (via 10k pull-up to 5V)
 *
 * Therefore, to achieve a fan duty of D%:
 *   CCR = ARR * (100 - D) / 100
 *
 * D=100% -> CCR=0      -> MCU always LOW -> fan always HIGH (full speed)
 * D=0%   -> CCR=ARR    -> MCU always HIGH -> fan always LOW (stopped)
 */

#include "fan_control.h"

/* Current duty per channel (0-100) */
static uint8_t current_duty[NUM_FANS];

/* Timer/channel mapping table */
typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t           channel;
} pwm_map_t;

static const pwm_map_t pwm_map[NUM_FANS] = {
    { &htim1,  TIM_CHANNEL_1 },  /* PWM1 = PA8 */
    { &htim3,  TIM_CHANNEL_2 },  /* PWM2 = PA7 */
    { &htim3,  TIM_CHANNEL_1 },  /* PWM3 = PA6 */
    { &htim2,  TIM_CHANNEL_1 },  /* PWM4 = PA5 */
    { &htim14, TIM_CHANNEL_1 },  /* PWM5 = PA4 */
};

void fan_control_init(void)
{
    for (uint8_t i = 0; i < NUM_FANS; i++)
        current_duty[i] = 0;
}

void fan_set_duty(uint8_t channel, uint8_t duty_percent)
{
    if (channel >= NUM_FANS)
        return;
    if (duty_percent > 100)
        duty_percent = 100;

    current_duty[channel] = duty_percent;

    /* Invert for NPN open-collector */
    uint32_t ccr = (uint32_t)PWM_ARR * (100U - duty_percent) / 100U;

    __HAL_TIM_SET_COMPARE(pwm_map[channel].htim,
                          pwm_map[channel].channel,
                          ccr);
}

uint8_t fan_get_duty(uint8_t channel)
{
    if (channel >= NUM_FANS)
        return 0;
    return current_duty[channel];
}

void fan_set_all(uint8_t duty_percent)
{
    for (uint8_t i = 0; i < NUM_FANS; i++)
        fan_set_duty(i, duty_percent);
}
