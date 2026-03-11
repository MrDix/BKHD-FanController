/**
 * @file watchdog.c
 * @brief Host communication watchdog (60s) and hardware IWDG.
 *
 * If no valid command arrives within HOST_TIMEOUT_MS (60s):
 *   - All fans set to 100%
 *   - Failsafe flag raised
 *
 * Hardware IWDG (~4s) resets MCU if firmware hangs.
 */

#include "watchdog.h"
#include "fan_control.h"

static uint32_t last_host_tick;
static bool failsafe_active;

void watchdog_init(void)
{
    last_host_tick = HAL_GetTick();
    failsafe_active = false;

    /* Configure IWDG: ~4 second timeout
     * LSI ~32 kHz, prescaler /256 -> 125 Hz
     * Reload = 500 -> 500/125 = 4 seconds */
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
    hiwdg.Init.Reload    = 500;
    hiwdg.Init.Window    = IWDG_WINDOW_DISABLE;
    HAL_IWDG_Init(&hiwdg);
}

void watchdog_host_feed(void)
{
    last_host_tick = HAL_GetTick();
}

void watchdog_check(void)
{
    uint32_t elapsed = HAL_GetTick() - last_host_tick;

    if (elapsed >= HOST_TIMEOUT_MS && !failsafe_active)
    {
        failsafe_active = true;
        fan_set_all(100);
    }
}

bool watchdog_is_failsafe(void)
{
    return failsafe_active;
}

void watchdog_clear_failsafe(void)
{
    failsafe_active = false;
    last_host_tick = HAL_GetTick();
}
