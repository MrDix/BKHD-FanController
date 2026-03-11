/**
 * @file tach_measure.c
 * @brief TACH pulse counting via EXTI + RPM calculation.
 *
 * Each fan TACH output generates 2 pulses per revolution (Intel 4-pin spec).
 * We count falling edges over a 1-second window and compute:
 *   RPM = (pulse_count / TACH_PULSES_PER_REV) * 60
 */

#include "tach_measure.h"
#include "fan_control.h"

/* Pulse counters — incremented in EXTI ISR, read in main loop */
static volatile uint32_t pulse_count[NUM_FANS];

/* Latched RPM values (updated every TACH_WINDOW_MS) */
static uint16_t rpm[NUM_FANS];

/* Timestamp of last measurement window */
static uint32_t last_window_tick;

/* Map EXTI pin to fan index */
static const uint16_t tach_pins[NUM_FANS] = {
    TACH1_PIN, TACH2_PIN, TACH3_PIN, TACH4_PIN, TACH5_PIN
};

void tach_init(void)
{
    for (uint8_t i = 0; i < NUM_FANS; i++)
    {
        pulse_count[i] = 0;
        rpm[i] = 0;
    }
    last_window_tick = HAL_GetTick();
}

void tach_update(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = now - last_window_tick;

    if (elapsed < TACH_WINDOW_MS)
        return;

    last_window_tick = now;

    for (uint8_t i = 0; i < NUM_FANS; i++)
    {
        /* Atomically read and clear counter */
        __disable_irq();
        uint32_t count = pulse_count[i];
        pulse_count[i] = 0;
        __enable_irq();

        /* RPM = (pulses / pulses_per_rev) * (60000 / elapsed_ms) */
        if (count > 0 && elapsed > 0)
            rpm[i] = (uint16_t)((count * 60000UL) /
                                (TACH_PULSES_PER_REV * elapsed));
        else
            rpm[i] = 0;
    }
}

uint16_t tach_get_rpm(uint8_t channel)
{
    if (channel >= NUM_FANS)
        return 0;
    return rpm[channel];
}

bool tach_fan_stalled(uint8_t channel)
{
    if (channel >= NUM_FANS)
        return false;

    /* Only consider a fan stalled if it should be spinning */
    if (fan_get_duty(channel) == 0)
        return false;

    return rpm[channel] < MIN_RPM_THRESHOLD;
}

/**
 * Called from HAL_GPIO_EXTI_Falling_Callback (in stm32g0xx_it.c).
 */
void tach_exti_callback(uint16_t pin)
{
    for (uint8_t i = 0; i < NUM_FANS; i++)
    {
        if (pin == tach_pins[i])
        {
            pulse_count[i]++;
            return;
        }
    }
}
