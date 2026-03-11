/**
 * @file stm32g0xx_it.c
 * @brief Interrupt handlers for the fan controller.
 */

#include "main.h"
#include "tach_measure.h"
#include "uart_protocol.h"

/* ---------- Cortex-M0+ system interrupts ---------- */

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    /* Turn on buzzer and halt */
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
    while (1) {}
}

void SVC_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ---------- EXTI interrupts (TACH signals) ---------- */

void EXTI0_1_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(TACH1_PIN);  /* PB0 = EXTI0 */
    HAL_GPIO_EXTI_IRQHandler(TACH2_PIN);  /* PB1 = EXTI1 */
}

void EXTI2_3_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(TACH3_PIN);  /* PB2 = EXTI2 */
    HAL_GPIO_EXTI_IRQHandler(TACH4_PIN);  /* PB3 = EXTI3 */
}

void EXTI4_15_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(TACH5_PIN);  /* PB4 = EXTI4 */
}

/**
 * HAL GPIO EXTI callback — routes to tach module.
 */
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    tach_exti_callback(GPIO_Pin);
}

/* ---------- USART2 interrupt ---------- */

void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

/**
 * HAL UART RX complete callback — routes to protocol module.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
        uart_rx_irq_handler();
}
