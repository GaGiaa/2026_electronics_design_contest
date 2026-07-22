#ifndef ENCODER_SPEED_FILTER_H
#define ENCODER_SPEED_FILTER_H

#include <stdint.h>

#define ENCODER_SPEED_FILTER_WINDOW_SAMPLES 5U
#define ENCODER_SPEED_FILTER_FRACTIONAL_BITS 8U

typedef struct {
    int32_t samples[ENCODER_SPEED_FILTER_WINDOW_SAMPLES];
    int32_t sum;
    uint32_t count;
    uint32_t next_index;
} encoder_speed_filter_t;

void encoder_speed_filter_init(encoder_speed_filter_t *filter);
int32_t encoder_speed_filter_update(encoder_speed_filter_t *filter,
                                    int32_t delta_counts);

#endif
