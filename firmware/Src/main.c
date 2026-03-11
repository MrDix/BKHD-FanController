/**
 * @file main.c
 * @brief Fan controller main — clock, GPIO, timer, UART init + main loop.
 *
 * Clock: HSI16 -> PLL -> 64 MHz SYSCLK
 * PWM:   25 kHz on TIM1/TIM2/TIM3/TIM14 (inverted for NPN open-collector)
 * TACH:  EXTI falling edge on PB0..PB4
 * UART:  USART2 @ 115200 8N1 on PA2/PA3
 * Buzzer: GPIO PA1
 */

#include "main.h"
#include "fan_control.h"
#include "tach_measure.h"
#include "uart_protocol.h"
#include "buzzer.h"
#include "watchdog.h"

/* ---------- Global HAL handles ---------- */
TIM_HandleTypeDef  htim1;
TIM_HandleTypeDef  htim2;
TIM_HandleTypeDef  htim3;
TIM_HandleTypeDef  htim14;
UART_HandleTypeDef huart2;
IWDG_HandleTypeDef hiwdg;

/* ---------- Forward declarations ---------- */
static void SystemClock_Config(void);
static void GPIO_Init(void);
static void TIM_PWM_Init(void);
static void UART_Init(void);

/* ---------- Main ---------- */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    TIM_PWM_Init();
    UART_Init();

    fan_control_init();
    tach_init();
    buzzer_init();
    uart_protocol_init();
    watchdog_init();

    /* Start with all fans at 100 % until host takes over */
    fan_set_all(100);

    uint32_t last_status = 0;

    while (1)
    {
        /* --- Process incoming UART commands --- */
        uart_process();

        uart_parsed_t cmd;
        if (uart_get_command(&cmd))
        {
            watchdog_host_feed();

            switch (cmd.cmd)
            {
            case CMD_SET:
                for (uint8_t i = 0; i < NUM_FANS; i++)
                    fan_set_duty(i, cmd.duty[i]);
                if (watchdog_is_failsafe())
                    watchdog_clear_failsafe();
                break;

            case CMD_KA:
                /* keep-alive, nothing else to do */
                break;

            case CMD_ACK:
                buzzer_off();
                break;

            default:
                break;
            }
        }

        /* --- TACH RPM update (every 1 s internally) --- */
        tach_update();

        /* --- Watchdog / failsafe check --- */
        watchdog_check();

        /* --- Alarm: check for stalled fans --- */
        {
            uint8_t err_mask = 0;
            for (uint8_t i = 0; i < NUM_FANS; i++)
            {
                if (tach_fan_stalled(i))
                    err_mask |= (1U << i);
            }

            if (err_mask || watchdog_is_failsafe())
                buzzer_blink_update();
            /* Note: buzzer_off() is only called via ACK command */

            /* --- Send status periodically --- */
            uint32_t now = HAL_GetTick();
            if (now - last_status >= STATUS_INTERVAL_MS)
            {
                last_status = now;

                uint16_t rpm[NUM_FANS];
                uint8_t  duty[NUM_FANS];
                for (uint8_t i = 0; i < NUM_FANS; i++)
                {
                    rpm[i]  = tach_get_rpm(i);
                    duty[i] = fan_get_duty(i);
                }

                uart_send_status(rpm, err_mask,
                                 watchdog_is_failsafe() ? 1 : 0,
                                 duty);
            }
        }

        /* --- Hardware watchdog refresh --- */
        HAL_IWDG_Refresh(&hiwdg);
    }
}

/* ---------- Clock: HSI16 -> PLL -> 64 MHz ---------- */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* Configure HSI and PLL */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState       = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM       = RCC_PLLM_DIV1;   /* /1 -> 16 MHz */
    osc.PLL.PLLN       = 8;                /* x8 -> 128 MHz VCO */
    osc.PLL.PLLR       = RCC_PLLR_DIV2;   /* /2 -> 64 MHz */
    osc.PLL.PLLP       = RCC_PLLP_DIV2;
    osc.PLL.PLLQ       = RCC_PLLQ_DIV2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler();

    /* Select PLL as SYSCLK, set prescalers */
    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                        | RCC_CLOCKTYPE_PCLK1;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK = 64 MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV1;     /* APB  = 64 MHz */

    /* 2 wait states for 64 MHz @ Range 1 */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
        Error_Handler();
}

/* ---------- GPIO init for non-AF pins ---------- */
static void GPIO_Init(void)
{
    /* Enable GPIO clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Buzzer: PA1, push-pull output, start LOW */
    GPIO_InitTypeDef gpio = {0};
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
    gpio.Pin   = BUZZER_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZER_PORT, &gpio);

    /* TACH inputs: PB0..PB4, EXTI falling edge, pull-up (external 10k exists,
       internal pull-up as additional safety) */
    gpio.Pin   = TACH1_PIN | TACH2_PIN | TACH3_PIN | TACH4_PIN | TACH5_PIN;
    gpio.Mode  = GPIO_MODE_IT_FALLING;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TACH_PORT, &gpio);

    /* Enable EXTI interrupts */
    HAL_NVIC_SetPriority(EXTI0_1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);
    HAL_NVIC_SetPriority(EXTI2_3_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);
    HAL_NVIC_SetPriority(EXTI4_15_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
}

/* ---------- Timer init for 25 kHz PWM ---------- */
static void TIM_PWM_Init(void)
{
    TIM_OC_InitTypeDef oc = {0};

    /* Common OC config: PWM Mode 1, active-high output.
     * The NPN inverter on the PCB produces correct fan PWM polarity.
     * Duty inversion is handled in fan_control.c via CCR = ARR - desired. */
    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 0;  /* 0 % initially */
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;

    /* ---- TIM1: PA8 = PWM1 (CH1) ---- */
    __HAL_RCC_TIM1_CLK_ENABLE();
    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = 0;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = PWM_ARR;
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim1);
    HAL_TIM_PWM_ConfigChannel(&htim1, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);

    /* ---- TIM3: PA6 = PWM3 (CH1), PA7 = PWM2 (CH2) ---- */
    __HAL_RCC_TIM3_CLK_ENABLE();
    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 0;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = PWM_ARR;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim3);
    HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);

    /* ---- TIM2: PA5 = PWM4 (CH1) ---- */
    __HAL_RCC_TIM2_CLK_ENABLE();
    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 0;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = PWM_ARR;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim2);
    HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

    /* ---- TIM14: PA4 = PWM5 (CH1) ---- */
    __HAL_RCC_TIM14_CLK_ENABLE();
    htim14.Instance               = TIM14;
    htim14.Init.Prescaler         = 0;
    htim14.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim14.Init.Period            = PWM_ARR;
    htim14.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim14);
    HAL_TIM_PWM_ConfigChannel(&htim14, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim14, TIM_CHANNEL_1);
}

/* ---------- USART2 init ---------- */
static void UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = UART_BAUDRATE;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK)
        Error_Handler();

    /* Enable RX interrupt */
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/* ---------- Error handler ---------- */
void Error_Handler(void)
{
    __disable_irq();
    /* Turn on buzzer to signal error */
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
    while (1) {}
}
