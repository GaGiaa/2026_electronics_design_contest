#include <assert.h>
#include <math.h>

#include "app_config.h"
#include "app_line_follow.h"

static void assert_close(float actual, float expected)
{
    assert(fabsf(actual - expected) < 0.0001f);
}

static PID_Position_Param_Config make_params(void)
{
    PID_Position_Param_Config params = {0};

    params.kp = 100.0f;
    params.output_limit = 250.0f;
    return params;
}

static void test_center_line_uses_requested_base_speed(void)
{
    app_line_follow_state_t state;
    app_line_follow_output_t output;
    const app_line_follow_input_t input = {
        .line_position = 0.0f,
        .line_strength = 4095U,
        .sequence = 1U,
        .base_speed_mm_s = 200.0f,
    };
    const PID_Position_Param_Config params = make_params();

    app_line_follow_init(&state, &params, 0.01f);
    app_line_follow_step(&state, &input, &output);

    assert(output.active);
    assert(output.line_valid);
    assert_close(output.turn_correction_mm_s, 0.0f);
    assert_close(output.left_target_rpm, 200.0f * APP_H723_MM_S_TO_OUTPUT_RPM);
    assert_close(output.right_target_rpm, -200.0f * APP_H723_MM_S_TO_OUTPUT_RPM);
}

static void test_new_grayscale_sequence_updates_turn_once(void)
{
    app_line_follow_state_t state;
    app_line_follow_output_t first;
    app_line_follow_output_t repeated;
    const PID_Position_Param_Config params = make_params();
    const app_line_follow_input_t first_input = {
        .line_position = 1.0f,
        .line_strength = 4095U,
        .sequence = 1U,
        .base_speed_mm_s = 200.0f,
    };
    const app_line_follow_input_t repeated_input = {
        .line_position = -1.0f,
        .line_strength = 4095U,
        .sequence = 1U,
        .base_speed_mm_s = 200.0f,
    };

    app_line_follow_init(&state, &params, 0.01f);
    app_line_follow_step(&state, &first_input, &first);
    app_line_follow_step(&state, &repeated_input, &repeated);

    assert_close(first.turn_correction_mm_s, 100.0f);
    assert_close(repeated.turn_correction_mm_s, first.turn_correction_mm_s);
    assert_close(repeated.left_target_rpm, first.left_target_rpm);
    assert_close(repeated.right_target_rpm, first.right_target_rpm);
}

static void test_invalid_grayscale_input_stops_output(void)
{
    app_line_follow_state_t state;
    app_line_follow_output_t output;
    const app_line_follow_input_t input = {
        .line_position = 1.0f,
        .line_strength = APP_H723_LINE_FOLLOW_LINE_STRENGTH_MIN - 1U,
        .sequence = 1U,
        .base_speed_mm_s = 200.0f,
    };
    const PID_Position_Param_Config params = make_params();

    app_line_follow_init(&state, &params, 0.01f);
    app_line_follow_step(&state, &input, &output);

    assert(!output.active);
    assert(!output.line_valid);
    assert_close(output.left_target_rpm, 0.0f);
    assert_close(output.right_target_rpm, 0.0f);
}

static void test_four_black_channels_latch_stop_until_reset(void)
{
    app_line_follow_state_t state;
    app_line_follow_output_t output;
    const PID_Position_Param_Config params = make_params();
    const app_line_follow_input_t running_input = {
        .line_strength = 4095U,
        .sequence = 1U,
        .base_speed_mm_s = 200.0f,
    };
    const app_line_follow_input_t stop_input = {
        .line_strength = 4095U,
        .sequence = 2U,
        .base_speed_mm_s = 200.0f,
        .black_count = APP_H723_LINE_FOLLOW_STOP_BLACK_COUNT,
    };
    const app_line_follow_input_t resumed_input = {
        .line_strength = 4095U,
        .sequence = 3U,
        .base_speed_mm_s = 200.0f,
    };

    app_line_follow_init(&state, &params, 0.01f);
    app_line_follow_step(&state, &running_input, &output);
    assert(output.active);

    app_line_follow_step(&state, &stop_input, &output);
    assert(!output.active);

    app_line_follow_step(&state, &resumed_input, &output);
    assert(!output.active);

    app_line_follow_reset(&state);
    app_line_follow_step(&state, &resumed_input, &output);
    assert(output.active);
}

int main(void)
{
    test_center_line_uses_requested_base_speed();
    test_new_grayscale_sequence_updates_turn_once();
    test_invalid_grayscale_input_stops_output();
    test_four_black_channels_latch_stop_until_reset();
    return 0;
}
