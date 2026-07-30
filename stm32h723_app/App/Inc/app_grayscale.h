#ifndef APP_GRAYSCALE_H
#define APP_GRAYSCALE_H

#include <stdbool.h>
#include <stdint.h>

#include "app_grayscale_math.h"

#define H723_GRAYSCALE_ADC_TIMEOUT_MS 1U

typedef struct {
    uint16_t raw[H723_GRAYSCALE_CHANNEL_COUNT];
    uint16_t normalized[H723_GRAYSCALE_CHANNEL_COUNT];
    uint8_t digital;
    uint8_t black_mask;
    uint8_t adc_timeout_mask;
    uint8_t black_count;
    uint32_t line_strength;
    int32_t line_error;
    float line_position;
    uint32_t sequence;
    uint32_t adc_timeout_count;
} h723_grayscale_snapshot_t;

void h723_grayscale_init(
    const uint16_t white[H723_GRAYSCALE_CHANNEL_COUNT],
    const uint16_t black[H723_GRAYSCALE_CHANNEL_COUNT]);
bool h723_grayscale_sample(h723_grayscale_snapshot_t *snapshot);

#endif
