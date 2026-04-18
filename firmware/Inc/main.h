/**
 * @file main.h
 * @brief Pin definitions and global constants for the fan controller.
 */
#ifndef MAIN_H
#define MAIN_H

#include "stm32g0xx_hal.h"

/* ---------- Build-time options ----------
 *
 * NTC1_ENABLED
 *   0 (default) -- stock board without NTC nachrüstung. PB7 is left as the
 *                  default post-reset state (analog input, no pullup/pulldown)
 *                  and neither the ADC peripheral nor the sampling code are
 *                  initialised. Every STS frame reports t1 = NTC_TEMP_INVALID
 *                  (INT16_MIN), so the host daemon's "ntc1" virtual sensor
 *                  stays absent and any fan configured against it falls back
 *                  to its fallback_sensor.
 *   1            -- NTC nachrüstung present: 100k pullup from +3V3 to PB7,
 *                  NTC from PB7 to GND. Enables ADC init + Beta-equation
 *                  conversion. Real temperature readings appear in t1.
 *
 * The option can be overridden on the CMake command line:
 *   cmake -B build -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi.cmake -DNTC1_ENABLED=1
 * or directly by editing this file.
 */
#ifndef NTC1_ENABLED
#define NTC1_ENABLED 0
#endif

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

/* ---------- NTC1 (ADC1_IN11 on PB7) ----------
 * Voltage divider: +3V3 -- R_PULLUP (100k) -- PB7 -- NTC -- GND
 * Sensor: Semitec 104NT-4-R025H42G (100k at 25 C, B=4267K)
 * Mounted with thermal adhesive pad on the Intel 82599ES heatsink.
 *
 * Only wired up when NTC1_ENABLED != 0. See build-time options above.
 */
#define NTC1_PIN          GPIO_PIN_7
#define NTC1_PORT         GPIOB
#define NTC1_ADC_CHANNEL  ADC_CHANNEL_11
#define NTC_PULLUP_OHMS   100000.0f
#define NTC_R25_OHMS      100000.0f
#define NTC_BETA_K        4267.0f
#define NTC_T25_KELVIN    298.15f

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
#if NTC1_ENABLED
extern ADC_HandleTypeDef  hadc1;
#endif

void Error_Handler(void);

#endif /* MAIN_H */
