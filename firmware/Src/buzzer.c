/**
 * @file buzzer.c
 * @brief Active buzzer control via GPIO.
 *
 * The active 5V buzzer is driven via NPN transistor from PA1.
 * HIGH = buzzer on, LOW = buzzer off.
 * Blink pattern: 200ms on / 800ms off for alarm indication.
 */

#include "buzzer.h"

static bool buzzer_active;
static uint32_t blink_tick;
static bool blink_state;

void buzzer_init(void)
{
    buzzer_active = false;
    blink_state = false;
    blink_tick = 0;
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}

void buzzer_on(void)
{
    buzzer_active = true;
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
}

void buzzer_off(void)
{
    buzzer_active = false;
    blink_state = false;
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}

void buzzer_set(bool active)
{
    if (active)
        buzzer_on();
    else
        buzzer_off();
}

void buzzer_blink_update(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t interval = blink_state ? 200 : 800;

    if (now - blink_tick >= interval)
    {
        blink_tick = now;
        blink_state = !blink_state;
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN,
                          blink_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}
