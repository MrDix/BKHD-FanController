/**
 * @file ntc.c
 * @brief NTC thermistor temperature measurement via ADC1 on PB7.
 *
 * Circuit (only populated when NTC1_ENABLED != 0):
 *   +3V3 --- R_PULLUP (100k) --- PB7 --- NTC (Semitec 104NT, 100k @ 25 C) --- GND
 *
 * ADC reads the midpoint voltage. With V_adc in [0, V_ref]:
 *   R_ntc = R_PULLUP * V_adc / (V_ref - V_adc)
 * Temperature via Beta equation:
 *   1/T = 1/T25 + (1/B) * ln(R_ntc / R25)
 *
 * When NTC1_ENABLED is 0 (stock PCB without the nachrüstung), this module
 * compiles down to trivial stubs so PB7 and ADC1 are never touched.
 */

#include "ntc.h"

#if NTC1_ENABLED

#include <math.h>

ADC_HandleTypeDef hadc1;

void ntc_init(void)
{
    __HAL_RCC_ADC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PB7 as analog input */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = NTC1_PIN;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(NTC1_PORT, &gpio);

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait      = DISABLE;
    hadc1.Init.LowPowerAutoPowerOff  = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.Overrun               = ADC_OVR_DATA_PRESERVED;
    hadc1.Init.SamplingTimeCommon1   = ADC_SAMPLETIME_160CYCLES_5;
    hadc1.Init.SamplingTimeCommon2   = ADC_SAMPLETIME_160CYCLES_5;
    hadc1.Init.OversamplingMode      = DISABLE;
    hadc1.Init.TriggerFrequencyMode  = ADC_TRIGGER_FREQ_LOW;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
        Error_Handler();

    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel      = NTC1_ADC_CHANNEL;
    ch.Rank         = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK)
        Error_Handler();

    /* Self-calibration (mandatory on STM32G0 for accuracy) */
    if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
        Error_Handler();
}

int16_t ntc_read_tenths_celsius(void)
{
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
        return NTC_TEMP_INVALID;

    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return NTC_TEMP_INVALID;
    }

    uint32_t raw = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    /* Guard against divide-by-zero and open/short conditions */
    if (raw == 0u || raw >= 4094u)
        return NTC_TEMP_INVALID;

    /* R_ntc = R_pullup * raw / (4095 - raw) */
    float v_frac = (float)raw / 4095.0f;
    float r_ntc  = NTC_PULLUP_OHMS * v_frac / (1.0f - v_frac);

    /* Beta equation: 1/T = 1/T25 + (1/B) * ln(R_ntc / R25) */
    float inv_t  = (1.0f / NTC_T25_KELVIN) +
                   (logf(r_ntc / NTC_R25_OHMS) / NTC_BETA_K);
    float t_c    = (1.0f / inv_t) - 273.15f;

    /* Clamp to representable range for int16 tenths (-3276.8 .. 3276.7 C) */
    if (t_c < -200.0f) return -2000;
    if (t_c >  200.0f) return  2000;

    return (int16_t)lroundf(t_c * 10.0f);
}

#else  /* NTC1_ENABLED == 0 */

void ntc_init(void)
{
    /* NTC nachrüstung not present: leave PB7 and ADC1 untouched. */
}

int16_t ntc_read_tenths_celsius(void)
{
    return NTC_TEMP_INVALID;
}

#endif /* NTC1_ENABLED */
