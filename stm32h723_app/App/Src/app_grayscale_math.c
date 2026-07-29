#include "app_grayscale_math.h"

#include <stddef.h>

static const int32_t s_line_weights[H723_GRAYSCALE_CHANNEL_COUNT] = {
    -3500, -2500, -1500, -500, 500, 1500, 2500, 3500
};

static uint16_t normalize(uint16_t value, uint16_t white, uint16_t black)
{
    uint32_t scaled;

    if (white <= black || value <= black) {
        return 0U;
    }
    if (value >= white) {
        return H723_GRAYSCALE_ADC_MAX;
    }
    scaled = ((uint32_t)(value - black) * H723_GRAYSCALE_ADC_MAX) /
             (uint32_t)(white - black);
    return scaled > H723_GRAYSCALE_ADC_MAX ? H723_GRAYSCALE_ADC_MAX :
                                               (uint16_t)scaled;
}

void h723_grayscale_derive(
    const uint16_t raw[H723_GRAYSCALE_CHANNEL_COUNT],
    const uint16_t white[H723_GRAYSCALE_CHANNEL_COUNT],
    const uint16_t black[H723_GRAYSCALE_CHANNEL_COUNT],
    uint8_t previous_digital,
    int32_t previous_line_error,
    h723_grayscale_derived_t *derived)
{
    uint32_t channel;
    int32_t weighted_sum = 0;
    uint32_t line_strength = 0U;

    if (raw == NULL || white == NULL || black == NULL || derived == NULL) {
        return;
    }

    derived->digital = previous_digital;
    for (channel = 0U; channel < H723_GRAYSCALE_CHANNEL_COUNT; ++channel) {
        uint16_t gray_white;
        uint16_t gray_black;
        uint32_t blackness;

        derived->normalized[channel] = normalize(raw[channel], white[channel],
                                                  black[channel]);
        if (white[channel] > black[channel]) {
            gray_white = (uint16_t)((black[channel] +
                                     (2U * white[channel])) / 3U);
            gray_black = (uint16_t)(((2U * black[channel]) +
                                     white[channel]) / 3U);
            if (raw[channel] >= gray_white) {
                derived->digital |= (uint8_t)(1U << channel);
            } else if (raw[channel] <= gray_black) {
                derived->digital &= (uint8_t)~(1U << channel);
            }
        }

        if (derived->normalized[channel] < H723_GRAYSCALE_ADC_MAX) {
            blackness = H723_GRAYSCALE_ADC_MAX -
                        derived->normalized[channel];
            line_strength += blackness;
            weighted_sum += (int32_t)blackness * s_line_weights[channel];
        }
    }

    derived->black_mask = (uint8_t)~derived->digital;
    derived->black_count = 0U;
    for (channel = 0U; channel < H723_GRAYSCALE_CHANNEL_COUNT; ++channel) {
        if ((derived->black_mask & (uint8_t)(1U << channel)) != 0U) {
            ++derived->black_count;
        }
    }
    derived->line_strength = line_strength;
    derived->line_error = line_strength != 0U ?
        weighted_sum / (int32_t)line_strength : previous_line_error;
}
