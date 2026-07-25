#include <assert.h>
#include <stdint.h>

#include "algorithms/line_tracking/line_tracking.h"

static void test_all_white_reports_lost_line(void)
{
    line_tracking_state_t state;
    line_tracking_result_t result;
    const uint16_t normalized[BOARD_GRAYSCALE_CHANNEL_COUNT] = {
        4095U, 4095U, 4095U, 4095U, 4095U, 4095U, 4095U, 4095U
    };

    line_tracking_init(&state, 1234);
    line_tracking_update(&state, normalized, 0xFFU, &result);

    assert(result.black_mask == 0U);
    assert(result.black_count == 0U);
    assert(result.line_strength == 0U);
    assert(result.line_error == 1234);
}

static void test_channel_zero_maps_to_low_weight(void)
{
    line_tracking_state_t state;
    line_tracking_result_t result;
    const uint16_t normalized[BOARD_GRAYSCALE_CHANNEL_COUNT] = {
        0U, 4095U, 4095U, 4095U, 4095U, 4095U, 4095U, 4095U
    };

    line_tracking_init(&state, 0);
    line_tracking_update(&state, normalized, 0xFEU, &result);

    assert(result.black_mask == 0x01U);
    assert(result.black_count == 1U);
    assert(result.line_strength == 4095U);
    assert(result.line_error == -3500);
}

static void test_black_line_moves_toward_positive_weights(void)
{
    line_tracking_state_t state;
    line_tracking_result_t left_result;
    line_tracking_result_t right_result;
    const uint16_t left_normalized[BOARD_GRAYSCALE_CHANNEL_COUNT] = {
        0U, 4095U, 4095U, 4095U, 4095U, 4095U, 4095U, 4095U
    };
    const uint16_t right_normalized[BOARD_GRAYSCALE_CHANNEL_COUNT] = {
        4095U, 4095U, 4095U, 4095U, 4095U, 4095U, 4095U, 0U
    };

    line_tracking_init(&state, 0);
    line_tracking_update(&state, left_normalized, 0xFEU, &left_result);
    line_tracking_update(&state, right_normalized, 0x7FU, &right_result);

    assert(left_result.line_error < right_result.line_error);
    assert(right_result.line_error == 3500);
}

static void test_all_black_is_fully_counted(void)
{
    line_tracking_state_t state;
    line_tracking_result_t result;
    const uint16_t normalized[BOARD_GRAYSCALE_CHANNEL_COUNT] = {
        0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
    };

    line_tracking_init(&state, 0);
    line_tracking_update(&state, normalized, 0x00U, &result);

    assert(result.black_mask == 0xFFU);
    assert(result.black_count == 8U);
    assert(result.line_strength == 32760U);
    assert(result.line_error == 0);
}

static void test_lost_line_holds_previous_error(void)
{
    line_tracking_state_t state;
    line_tracking_result_t result;
    const uint16_t black_normalized[BOARD_GRAYSCALE_CHANNEL_COUNT] = {
        4095U, 4095U, 4095U, 4095U, 4095U, 4095U, 0U, 4095U
    };
    const uint16_t white_normalized[BOARD_GRAYSCALE_CHANNEL_COUNT] = {
        4095U, 4095U, 4095U, 4095U, 4095U, 4095U, 4095U, 4095U
    };

    line_tracking_init(&state, 0);
    line_tracking_update(&state, black_normalized, 0xBFU, &result);
    assert(result.line_error == 2500);
    line_tracking_update(&state, white_normalized, 0xFFU, &result);

    assert(result.line_strength == 0U);
    assert(result.line_error == 2500);
}

int main(void)
{
    test_all_white_reports_lost_line();
    test_channel_zero_maps_to_low_weight();
    test_black_line_moves_toward_positive_weights();
    test_all_black_is_fully_counted();
    test_lost_line_holds_previous_error();
    return 0;
}
