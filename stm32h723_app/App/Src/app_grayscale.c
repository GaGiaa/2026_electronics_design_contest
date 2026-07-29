#include "app_grayscale.h"

#include <stddef.h>

#include "adc.h"
#include "gpio.h"

#define H723_GRAYSCALE_AD0_PIN GPIO_PIN_3
#define H723_GRAYSCALE_AD1_PIN GPIO_PIN_4
#define H723_GRAYSCALE_AD2_PIN GPIO_PIN_5

static uint16_t s_white[H723_GRAYSCALE_CHANNEL_COUNT];
static uint16_t s_black[H723_GRAYSCALE_CHANNEL_COUNT];
static uint8_t s_digital;
static int32_t s_line_error;
static uint32_t s_sequence;
static uint32_t s_adc_timeout_count;

static void set_address(uint8_t channel)
{
    HAL_GPIO_WritePin(GPIOG, H723_GRAYSCALE_AD0_PIN,
                      (channel & 0x01U) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOG, H723_GRAYSCALE_AD1_PIN,
                      (channel & 0x02U) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOG, H723_GRAYSCALE_AD2_PIN,
                      (channel & 0x04U) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
    for (volatile uint32_t delay = 32U; delay != 0U; --delay) {
        __NOP();
    }
}

static bool read_average(uint16_t *value)
{
    uint32_t total = 0U;
    uint32_t sample;

    if (value == NULL) {
        return false;
    }
    for (sample = 0U; sample < 8U; ++sample) {
        if (HAL_ADC_Start(&hadc1) != HAL_OK) {
            return false;
        }
        if (HAL_ADC_PollForConversion(&hadc1,
                                      H723_GRAYSCALE_ADC_TIMEOUT_MS) != HAL_OK) {
            (void)HAL_ADC_Stop(&hadc1);
            return false;
        }
        total += HAL_ADC_GetValue(&hadc1);
        (void)HAL_ADC_Stop(&hadc1);
    }
    *value = (uint16_t)(total / 8U);
    return true;
}

void h723_grayscale_init(
    const uint16_t white[H723_GRAYSCALE_CHANNEL_COUNT],
    const uint16_t black[H723_GRAYSCALE_CHANNEL_COUNT])
{
    uint32_t channel;

    s_digital = 0U;
    s_line_error = 0;
    s_sequence = 0U;
    s_adc_timeout_count = 0U;
    for (channel = 0U; channel < H723_GRAYSCALE_CHANNEL_COUNT; ++channel) {
        s_white[channel] = white != NULL ? white[channel] : 0U;
        s_black[channel] = black != NULL ? black[channel] : 0U;
    }
}

bool h723_grayscale_sample(h723_grayscale_snapshot_t *snapshot)
{
    h723_grayscale_derived_t derived = {0};
    uint32_t channel;

    if (snapshot == NULL) {
        return false;
    }
    snapshot->adc_timeout_mask = 0U;
    for (channel = 0U; channel < H723_GRAYSCALE_CHANNEL_COUNT; ++channel) {
        set_address((uint8_t)channel);
        if (!read_average(&snapshot->raw[channel])) {
            snapshot->raw[channel] = 0U;
            snapshot->adc_timeout_mask |= (uint8_t)(1U << channel);
            ++s_adc_timeout_count;
        }
    }

    h723_grayscale_derive(snapshot->raw, s_white, s_black, s_digital,
                           s_line_error, &derived);
    for (channel = 0U; channel < H723_GRAYSCALE_CHANNEL_COUNT; ++channel) {
        snapshot->normalized[channel] = derived.normalized[channel];
    }
    snapshot->digital = derived.digital;
    snapshot->black_mask = derived.black_mask;
    snapshot->black_count = derived.black_count;
    snapshot->line_strength = derived.line_strength;
    snapshot->line_error = derived.line_error;
    snapshot->sequence = ++s_sequence;
    snapshot->adc_timeout_count = s_adc_timeout_count;
    s_digital = derived.digital;
    s_line_error = derived.line_error;
    return snapshot->adc_timeout_mask == 0U;
}
