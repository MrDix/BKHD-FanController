/**
 * @file stm32g0xx_hal_conf.h
 * @brief HAL configuration — enable only the modules we need.
 */
#ifndef STM32G0xx_HAL_CONF_H
#define STM32G0xx_HAL_CONF_H

/* ---- Module selection ---- */
#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_IWDG_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* ---- Oscillator values ---- */
#define HSI_VALUE            16000000UL   /* HSI 16 MHz */
#define LSI_VALUE               32000UL   /* LSI ~32 kHz */
#define HSE_VALUE            8000000UL    /* not used, placeholder */
#define HSE_STARTUP_TIMEOUT      100UL
#define LSE_VALUE               32768UL
#define LSE_STARTUP_TIMEOUT     5000UL
#define EXTERNAL_I2S1_CLOCK_VALUE 0UL

/* ---- System tick ---- */
#define TICK_INT_PRIORITY    3U
#define USE_RTOS             0U
#define PREFETCH_ENABLE      1U
#define INSTRUCTION_CACHE_ENABLE 1U

/* ---- Assert (disabled in release) ---- */
/* #define USE_FULL_ASSERT 1U */

/* ---- Include guards for enabled modules ---- */
#ifdef HAL_RCC_MODULE_ENABLED
 #include "stm32g0xx_hal_rcc.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
 #include "stm32g0xx_hal_gpio.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
 #include "stm32g0xx_hal_pwr.h"
 #include "stm32g0xx_hal_pwr_ex.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
 #include "stm32g0xx_hal_cortex.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
 #include "stm32g0xx_hal_flash.h"
 #include "stm32g0xx_hal_flash_ex.h"
#endif
#ifdef HAL_IWDG_MODULE_ENABLED
 #include "stm32g0xx_hal_iwdg.h"
#endif
#ifdef HAL_TIM_MODULE_ENABLED
 #include "stm32g0xx_hal_tim.h"
 #include "stm32g0xx_hal_tim_ex.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
 #include "stm32g0xx_hal_uart.h"
 #include "stm32g0xx_hal_uart_ex.h"
#endif

#endif /* STM32G0xx_HAL_CONF_H */
