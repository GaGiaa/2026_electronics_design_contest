#include <assert.h>
#include <math.h>
#include <stddef.h>

#include "line_control.h"

static void assert_close(float actual, float expected, float tolerance)
{
    assert(fabsf(actual - expected) <= tolerance);
}

static void reset_debug(void)
{
    g_line_control_debug.use_pid_override = true;
    g_line_control_debug.pid_params.kp = 0.08f;
    g_line_control_debug.pid_params.ki = 0.0f;
    g_line_control_debug.pid_params.kd = 0.0f;
    g_line_control_debug.pid_params.output_limit = 300.0f;
    g_line_control_debug.pid_params.deadband = 0.0f;
    g_line_control_debug.max_turn_speed_mm_per_s = 300.0f;
    g_line_control_debug.max_wheel_speed_mm_per_s = 800.0f;
    g_line_control_debug.turn_sign = 1.0f;
    g_line_control_debug.line_strength_enter = 800U;
    g_line_control_debug.line_strength_exit = 400U;
    g_line_control_debug.lost_line_timeout_ms = 100U;
}

static line_control_input_t make_input(int32_t error, uint32_t strength,
                                       uint8_t timeout_mask, uint32_t sequence,
                                       float base_speed, uint32_t now_ms)
{
    line_control_input_t input = {
        .line_error = error,
        .line_strength = strength,
        .adc_timeout_mask = timeout_mask,
        .sequence = sequence,
        .base_speed_mm_per_s = base_speed,
        .now_ms = now_ms,
    };
    return input;
}

static void test_center_line_keeps_left_and_right_equal(void)
{
    line_control_state_t state;
    line_control_output_t output;
    line_control_input_t input;

    reset_debug();
    line_control_init(&state);
    input = make_input(0, 4095U, 0U, 1U, 200.0f, 0U);
    line_control_step(&state, &input, &output);

    assert(output.line_valid);
    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT], 200.0f, 0.001f);
    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_RIGHT], 200.0f, 0.001f);
}

static void test_positive_error_turns_right_and_negative_error_turns_left(void)
{
    line_control_state_t state;
    line_control_output_t output;
    line_control_input_t input;

    reset_debug();
    line_control_init(&state);
    input = make_input(1000, 4095U, 0U, 1U, 200.0f, 0U);
    line_control_step(&state, &input, &output);
    assert(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT] <
           output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_RIGHT]);

    input = make_input(-1000, 4095U, 0U, 2U, 200.0f, 10U);
    line_control_step(&state, &input, &output);
    assert(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT] >
           output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_RIGHT]);
}

static void test_wheel_targets_are_scaled_to_maximum_speed(void)
{
    line_control_state_t state;
    line_control_output_t output;
    line_control_input_t input;

    reset_debug();
    line_control_init(&state);
    input = make_input(3500, 4095U, 0U, 1U, 800.0f, 0U);
    line_control_step(&state, &input, &output);

    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_RIGHT], 800.0f, 0.001f);
    assert(fabsf(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT]) < 800.0f);
}

static void test_invalid_line_stops_before_first_valid_sample(void)
{
    line_control_state_t state;
    line_control_output_t output;
    line_control_input_t input;

    reset_debug();
    line_control_init(&state);
    input = make_input(0, 0U, 0U, 1U, 200.0f, 0U);
    line_control_step(&state, &input, &output);

    assert(!output.line_valid);
    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT], 0.0f, 0.001f);
    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_RIGHT], 0.0f, 0.001f);
}

static void test_lost_line_holds_then_stops_and_resets(void)
{
    line_control_state_t state;
    line_control_output_t output;
    line_control_input_t input;
    float held_left;

    reset_debug();
    line_control_init(&state);
    input = make_input(1000, 4095U, 0U, 1U, 200.0f, 0U);
    line_control_step(&state, &input, &output);
    held_left = output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT];

    input = make_input(1000, 0U, 0U, 2U, 200.0f, 10U);
    line_control_step(&state, &input, &output);
    assert(!output.line_valid);
    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT], held_left, 0.001f);

    input = make_input(1000, 0U, 0U, 3U, 200.0f, 101U);
    line_control_step(&state, &input, &output);
    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT], 0.0f, 0.001f);
    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_RIGHT], 0.0f, 0.001f);

    input = make_input(0, 4095U, 0U, 4U, 200.0f, 110U);
    line_control_step(&state, &input, &output);
    assert(output.line_valid);
    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT], 200.0f, 0.001f);
}

static void test_adc_timeout_is_invalid_even_when_strength_is_high(void)
{
    line_control_state_t state;
    line_control_output_t output;
    line_control_input_t input;

    reset_debug();
    line_control_init(&state);
    input = make_input(0, 4095U, 0x01U, 1U, 200.0f, 0U);
    line_control_step(&state, &input, &output);

    assert(!output.line_valid);
    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT], 0.0f, 0.001f);
}

static void test_repeated_sequence_does_not_update_pid(void)
{
    line_control_state_t state;
    line_control_output_t first;
    line_control_output_t repeated;
    line_control_input_t input;

    reset_debug();
    line_control_init(&state);
    input = make_input(1000, 4095U, 0U, 1U, 200.0f, 0U);
    line_control_step(&state, &input, &first);
    input = make_input(0, 4095U, 0U, 1U, 200.0f, 10U);
    line_control_step(&state, &input, &repeated);

    assert_close(repeated.turn_speed_mm_per_s, first.turn_speed_mm_per_s, 0.001f);
    assert_close(repeated.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT],
                 first.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT], 0.001f);
}

int main(void)
{
    test_center_line_keeps_left_and_right_equal();
    test_positive_error_turns_right_and_negative_error_turns_left();
    test_wheel_targets_are_scaled_to_maximum_speed();
    test_invalid_line_stops_before_first_valid_sample();
    test_lost_line_holds_then_stops_and_resets();
    test_adc_timeout_is_invalid_even_when_strength_is_high();
    test_repeated_sequence_does_not_update_pid();
    return 0;
}
