#ifndef LINE_TRACKING_H
#define LINE_TRACKING_H

#include <stdint.h>

#include "board_grayscale.h"

#define LINE_TRACKING_MIN_STRENGTH 1U

typedef struct {
    int32_t last_error;
} line_tracking_state_t;

typedef struct {
    uint8_t black_mask;
    uint8_t black_count;
    uint32_t line_strength;
    int32_t line_error;
} line_tracking_result_t;

void line_tracking_init(line_tracking_state_t *state, int32_t initial_error);
void line_tracking_update(line_tracking_state_t *state,
                          const uint16_t normalized[BOARD_GRAYSCALE_CHANNEL_COUNT],
                          uint8_t digital,
                          line_tracking_result_t *result);

#endif
