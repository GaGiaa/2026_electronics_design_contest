#include "line_tracking.h"

#include <stddef.h>

static const int32_t g_line_tracking_weights[BOARD_GRAYSCALE_CHANNEL_COUNT] = {
    -3500, -2500, -1500, -500, 500, 1500, 2500, 3500
};

void line_tracking_init(line_tracking_state_t *state, int32_t initial_error)
{
    if (state == NULL) {
        return;
    }
    state->last_error = initial_error;
}

void line_tracking_update(line_tracking_state_t *state,
                          const uint16_t normalized[BOARD_GRAYSCALE_CHANNEL_COUNT],
                          uint8_t digital,
                          line_tracking_result_t *result)
{
    uint32_t channel;
    int32_t weighted_sum = 0;
    uint32_t line_strength = 0U;

    if ((state == NULL) || (normalized == NULL) || (result == NULL)) {
        return;
    }

    result->black_mask = (uint8_t)~digital;
    result->black_count = 0U;
    for (channel = 0U; channel < BOARD_GRAYSCALE_CHANNEL_COUNT; ++channel) {
        if ((result->black_mask & (uint8_t)(1U << channel)) != 0U) {
            ++result->black_count;
        }
        if (normalized[channel] < BOARD_GRAYSCALE_ADC_MAX) {
            uint32_t blackness = BOARD_GRAYSCALE_ADC_MAX - normalized[channel];
            line_strength += blackness;
            weighted_sum += (int32_t)blackness * g_line_tracking_weights[channel];
        }
    }
    result->line_strength = line_strength;
    if (line_strength >= LINE_TRACKING_MIN_STRENGTH) {
        state->last_error = weighted_sum / (int32_t)line_strength;
    }
    result->line_error = state->last_error;
}
