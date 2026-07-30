#ifndef APP_GRAYSCALE_MATH_H
#define APP_GRAYSCALE_MATH_H

#include <stdint.h>

#define H723_GRAYSCALE_CHANNEL_COUNT 8U
#define H723_GRAYSCALE_ADC_MAX 4095U

typedef struct {
    uint16_t normalized[H723_GRAYSCALE_CHANNEL_COUNT];
    uint8_t digital;
    uint8_t black_mask;
    uint8_t black_count;
    uint32_t line_strength;
    int32_t line_error;
    float line_position;
} h723_grayscale_derived_t;

void h723_grayscale_derive(
    const uint16_t raw[H723_GRAYSCALE_CHANNEL_COUNT],
    const uint16_t white[H723_GRAYSCALE_CHANNEL_COUNT],
    const uint16_t black[H723_GRAYSCALE_CHANNEL_COUNT],
    uint8_t previous_digital,
    int32_t previous_line_error,
    h723_grayscale_derived_t *derived);

#endif
