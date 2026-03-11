/**
 * @file main.h
 * @brief Pin definitions and global constants for the fan controller.
 */
#ifndef MAIN_H
#define MAIN_H

#include "stm32g0xx_hal.h"

/* ---------- Number of fan channels ---------- */
#define NUM_FANS  5

/* ---------- PWM pins (active-low via NPN open-collector) ---------- */
/*  PWM1 = PA8  (TIM1_CH1,  AF2)
 *  PWM2 = PA7  (TIM3_CH2,  AF1)
 *  PWM3 = PA6  (TIM3_CH1,  AF1)
 *  PWM4 = PA5  (TIM2_CH1,  AF2)
 *  PWM5 = PA4  (TIM14_CH1, AF4)
 */
#define PWM1_PIN   GPIO_PIN_8
#define PWM2_PIN   GPIO_PIN_7
#define PWM3_PIN   GPIO_PIN_6
#define PWM4_PIN   GPIO_PIN_5
#define PWM5_PIN   GPIO_PIN_4
#define PWM_PORT   GPIOA

/* ---------- TACH pins (EXTI falling edge) ---------- */
/*  TACH1 = PB0  (EXTI0)
 *  TACH2 = PB1  (EXTI1)
 *  TACH3 = PB2  (EXTI2)
 *  TACH4 = PB3  (EXTI3)
 *  TACH5 = PB4  (EXTI4)
 */
#define TACH1_PIN  GPIO_PIN_0
#define TACH2_PIN  GPIO_PIN_1
#define TACH3_PIN  GPIO_PIN_2
#define TACH4_PIN  GPIO_PIN_3
#define TACH5_PIN  GPIO_PIN_4
#define TACH_PORT  GPIOB

/* ---------- Buzzer ---------- */
#define BUZZER_PIN  GPIO_PIN_1
#define BUZZER_PORT GPIOA

/* ---------- UART (USART2) ---------- */
#define UART_TX_PIN GPIO_PIN_2
#define UART_RX_PIN GPIO_PIN_3
#define UART_PORT   GPIOA

/* ---------- PWM parameters ---------- */
#define PWM_FREQ_HZ       25000U
#define SYSCLK_HZ         64000000UL
/* ARR = SYSCLK / PWM_FREQ - 1 = 2559 */
#define PWM_ARR            ((SYSCLK_HZ / PWM_FREQ_HZ) - 1U)

/* ---------- UART parameters ---------- */
#define UART_BAUDRATE      115200U

/* ---------- Timing (ms) ---------- */
#define STATUS_INTERVAL_MS     500U
#define TACH_WINDOW_MS        1000U
#define HOST_TIMEOUT_MS      60000U

/* ---------- TACH ---------- */
#define TACH_PULSES_PER_REV    2U
#define MIN_RPM_THRESHOLD    200U

/* ---------- Global handles (defined in main.c) ---------- */
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim14;
extern UART_HandleTypeDef huart2;
extern IWDG_HandleTypeDef hiwdg;

void Error_Handler(void);

#endif /* MAIN_H */
