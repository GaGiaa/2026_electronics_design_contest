#include "encoder_speed_filter.h"

#include <stddef.h>

void encoder_speed_filter_init(encoder_speed_filter_t *filter)
{
    uint32_t index;

    if (filter == NULL) {
        return;
    }

    for (index = 0U; index < ENCODER_SPEED_FILTER_WINDOW_SAMPLES; ++index) {
        filter->samples[index] = 0;
    }
    filter->sum = 0;
    filter->count = 0U;
    filter->next_index = 0U;
}

int32_t encoder_speed_filter_update(encoder_speed_filter_t *filter,
                                    int32_t delta_counts)
{
    int32_t average_counts_q8;
    int64_t scaled_sum;

    if (filter == NULL) {
        return 0;
    }

    if (filter->count < ENCODER_SPEED_FILTER_WINDOW_SAMPLES) {
        filter->samples[filter->next_index] = delta_counts;
        filter->sum += delta_counts;
        ++filter->count;
    } else {
        filter->sum -= filter->samples[filter->next_index];
        filter->samples[filter->next_index] = delta_counts;
        filter->sum += delta_counts;
    }

    ++filter->next_index;
    if (filter->next_index >= ENCODER_SPEED_FILTER_WINDOW_SAMPLES) {
        filter->next_index = 0U;
    }

    scaled_sum = (int64_t)filter->sum *
                 (int64_t)(1U << ENCODER_SPEED_FILTER_FRACTIONAL_BITS);
    average_counts_q8 = (int32_t)(scaled_sum / (int64_t)filter->count);
    return average_counts_q8;
}
