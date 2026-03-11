/**
 * @file system_stm32g0xx.c
 * @brief CMSIS Cortex-M0+ system source — SystemInit and SystemCoreClock.
 *
 * This is a minimal implementation. The actual clock config is done
 * in main.c via HAL_RCC_OscConfig / HAL_RCC_ClockConfig.
 */

#include "stm32g0xx.h"

/* Initial system clock after reset (HSI16 = 16 MHz) */
uint32_t SystemCoreClock = 16000000UL;

const uint32_t AHBPrescTable[16] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 2, 3, 4, 6, 7, 8, 9
};

const uint32_t APBPrescTable[8] = {
    0, 0, 0, 0, 1, 2, 3, 4
};

/**
 * Called by startup code before main().
 * Keep it minimal — HAL_Init + SystemClock_Config in main() does the real work.
 */
void SystemInit(void)
{
    /* Nothing to do — defaults are fine for HSI16 boot. */
}
