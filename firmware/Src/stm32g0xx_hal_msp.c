/**
 * @file stm32g0xx_hal_msp.c
 * @brief HAL MSP (MCU Support Package) callbacks.
 *
 * These are called by HAL_XXX_Init() to configure the low-level hardware
 * (GPIO alternate functions, clocks, etc.) for each peripheral.
 */

#include "main.h"

/**
 * Global MSP init — called by HAL_Init().
 */
void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

/**
 * UART MSP init — configure PA2 (TX) and PA3 (RX) as USART2 AF1.
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitTypeDef gpio = {0};
        gpio.Pin       = UART_TX_PIN | UART_RX_PIN;
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_PULLUP;
        gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
        gpio.Alternate = GPIO_AF1_USART2;
        HAL_GPIO_Init(UART_PORT, &gpio);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        __HAL_RCC_USART2_CLK_DISABLE();
        HAL_GPIO_DeInit(UART_PORT, UART_TX_PIN | UART_RX_PIN);
    }
}

/**
 * TIM PWM MSP init — configure GPIO pins as timer AF outputs.
 */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    if (htim->Instance == TIM1)
    {
        /* PA8 = TIM1_CH1, AF2 */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        gpio.Pin       = PWM1_PIN;
        gpio.Alternate = GPIO_AF2_TIM1;
        HAL_GPIO_Init(PWM_PORT, &gpio);
    }
    else if (htim->Instance == TIM3)
    {
        /* PA6 = TIM3_CH1, PA7 = TIM3_CH2, AF1 */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        gpio.Pin       = PWM3_PIN | PWM2_PIN;
        gpio.Alternate = GPIO_AF1_TIM3;
        HAL_GPIO_Init(PWM_PORT, &gpio);
    }
    else if (htim->Instance == TIM2)
    {
        /* PA5 = TIM2_CH1, AF2 */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        gpio.Pin       = PWM4_PIN;
        gpio.Alternate = GPIO_AF2_TIM2;
        HAL_GPIO_Init(PWM_PORT, &gpio);
    }
    else if (htim->Instance == TIM14)
    {
        /* PA4 = TIM14_CH1, AF4 */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        gpio.Pin       = PWM5_PIN;
        gpio.Alternate = GPIO_AF4_TIM14;
        HAL_GPIO_Init(PWM_PORT, &gpio);
    }
}
