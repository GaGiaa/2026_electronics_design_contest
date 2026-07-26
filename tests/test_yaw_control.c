#include <assert.h>
#include <math.h>

#include "config/app_config.h"
#include "yaw_control.h"

static void assert_close(float actual, float expected, float tolerance)
{
    assert(fabsf(actual - expected) <= tolerance);
}

static void reset_debug(void)
{
    g_yaw_control_debug.use_pid_override = true;
    g_yaw_control_debug.target_yaw_deg = 0.0f;
    g_yaw_control_debug.pid_params.kp = 0.4f;
    g_yaw_control_debug.pid_params.ki = 0.0f;
    g_yaw_control_debug.pid_params.kd = 0.0f;
    g_yaw_control_debug.pid_params.output_limit = 300.0f;
    g_yaw_control_debug.pid_params.deadband = 0.0f;
    g_yaw_control_debug.max_turn_speed_mm_per_s = 300.0f;
    g_yaw_control_debug.max_wheel_speed_mm_per_s = 800.0f;
    g_yaw_control_debug.turn_sign = -1.0f;
}

static yaw_control_input_t make_input(float yaw_deg, bool valid,
                                      float base_speed, uint32_t now_ms)
{
    yaw_control_input_t input = {
        .feedback_yaw_deg = yaw_deg,
        .feedback_valid = valid,
        .base_speed_mm_per_s = base_speed,
        .now_ms = now_ms,
    };
    return input;
}

static void test_compiled_defaults_are_applied(void)
{
    yaw_control_state_t state;
    yaw_control_output_t output;
    yaw_control_input_t input;

    reset_debug();
    g_yaw_control_debug.use_pid_override = false;
    g_yaw_control_debug.max_turn_speed_mm_per_s = 700.0f;
    yaw_control_init(&state);

    input = make_input(-60.0f, true, 0.0f, 0U);
    yaw_control_step(&state, &input, &output);
    assert_close(output.turn_speed_mm_per_s, -700.0f, 0.001f);

    input = make_input(-0.05f, true, 0.0f, 50U);
    yaw_control_step(&state, &input, &output);
    assert_close(output.turn_speed_mm_per_s, 0.0f, 0.001f);
}

static void test_first_valid_sample_calculates_and_mixes(void)
{
    yaw_control_state_t state;
    yaw_control_output_t output;
    yaw_control_input_t input;

    reset_debug();
    yaw_control_init(&state);
    input = make_input(10.0f, true, 200.0f, 0U);
    yaw_control_step(&state, &input, &output);

    assert(output.yaw_valid);
    assert_close(output.yaw_error_deg, -10.0f, 0.001f);
    assert_close(output.turn_speed_mm_per_s, 4.0f, 0.001f);
    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT],
                 204.0f, 0.001f);
    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_RIGHT],
                 196.0f, 0.001f);
}

static void test_position_loop_updates_only_every_50_ms(void)
{
    yaw_control_state_t state;
    yaw_control_output_t output;
    yaw_control_input_t input;

    reset_debug();
    yaw_control_init(&state);
    input = make_input(0.0f, true, 200.0f, 0U);
    yaw_control_step(&state, &input, &output);

    input = make_input(20.0f, true, 200.0f, 49U);
    yaw_control_step(&state, &input, &output);
    assert_close(output.yaw_error_deg, -20.0f, 0.001f);
    assert_close(output.turn_speed_mm_per_s, 0.0f, 0.001f);

    input = make_input(20.0f, true, 200.0f, 50U);
    yaw_control_step(&state, &input, &output);
    assert_close(output.turn_speed_mm_per_s, 8.0f, 0.001f);
}

static void test_yaw_error_wraps_across_boundary(void)
{
    yaw_control_state_t state;
    yaw_control_output_t output;
    yaw_control_input_t input;

    reset_debug();
    g_yaw_control_debug.target_yaw_deg = -179.0f;
    yaw_control_init(&state);
    input = make_input(179.0f, true, 0.0f, 0U);
    yaw_control_step(&state, &input, &output);

    assert_close(output.yaw_error_deg, 2.0f, 0.001f);
    assert_close(output.turn_speed_mm_per_s, -0.8f, 0.001f);
}

static void test_yaw_error_wrap_does_not_spike_derivative(void)
{
    yaw_control_state_t state;
    yaw_control_output_t output;
    yaw_control_input_t input;

    reset_debug();
    g_yaw_control_debug.pid_params.kp = 0.0f;
    g_yaw_control_debug.pid_params.kd = 1.0f;
    g_yaw_control_debug.pid_params.output_limit = 10000.0f;
    g_yaw_control_debug.max_turn_speed_mm_per_s = 10000.0f;
    yaw_control_init(&state);

    input = make_input(-179.0f, true, 0.0f, 0U);
    yaw_control_step(&state, &input, &output);
    input = make_input(179.0f, true, 0.0f, 50U);
    yaw_control_step(&state, &input, &output);

    assert_close(output.yaw_error_deg, -179.0f, 0.001f);
    assert_close(output.pid_d_out, 0.0f, 0.001f);
    assert_close(output.turn_speed_mm_per_s, 0.0f, 0.001f);
}

static void test_target_changes_take_effect_at_next_position_update(void)
{
    yaw_control_state_t state;
    yaw_control_output_t output;
    yaw_control_input_t input;

    reset_debug();
    yaw_control_init(&state);
    input = make_input(0.0f, true, 0.0f, 0U);
    yaw_control_step(&state, &input, &output);

    g_yaw_control_debug.target_yaw_deg = 90.0f;
    input.now_ms = 50U;
    yaw_control_step(&state, &input, &output);
    assert_close(output.yaw_error_deg, 90.0f, 0.001f);
    assert_close(output.turn_speed_mm_per_s, -36.0f, 0.001f);
}

static void test_invalid_feedback_stops_and_recovery_restarts_pid(void)
{
    yaw_control_state_t state;
    yaw_control_output_t output;
    yaw_control_input_t input;

    reset_debug();
    yaw_control_init(&state);
    input = make_input(20.0f, true, 200.0f, 0U);
    yaw_control_step(&state, &input, &output);
    assert(fabsf(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT]) >
           0.0f);

    input = make_input(20.0f, false, 200.0f, 10U);
    yaw_control_step(&state, &input, &output);
    assert(!output.yaw_valid);
    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT],
                 0.0f, 0.001f);
    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_RIGHT],
                 0.0f, 0.001f);

    input = make_input(20.0f, true, 200.0f, 20U);
    yaw_control_step(&state, &input, &output);
    assert(output.yaw_valid);
    assert_close(output.turn_speed_mm_per_s, 8.0f, 0.001f);
}

static void test_wheel_targets_scale_to_maximum_speed(void)
{
    yaw_control_state_t state;
    yaw_control_output_t output;
    yaw_control_input_t input;

    reset_debug();
    g_yaw_control_debug.pid_params.kp = 2.0f;
    yaw_control_init(&state);
    input = make_input(-179.0f, true, 800.0f, 0U);
    yaw_control_step(&state, &input, &output);

    assert_close(output.turn_speed_mm_per_s, -300.0f, 0.001f);
    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT],
                 363.63635f, 0.01f);
    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_RIGHT],
                 800.0f, 0.001f);
}

static void test_invalid_debug_pid_stops_safely(void)
{
    yaw_control_state_t state;
    yaw_control_output_t output;
    yaw_control_input_t input;

    reset_debug();
    g_yaw_control_debug.pid_params.kp = -1.0f;
    yaw_control_init(&state);
    input = make_input(10.0f, true, 0.0f, 0U);
    yaw_control_step(&state, &input, &output);

    assert(!output.yaw_valid);
    assert_close(output.turn_speed_mm_per_s, 0.0f, 0.001f);
    assert_close(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT],
                 0.0f, 0.001f);
}

int main(void)
{
    assert(APP_YAW_CONTROL_INTERVAL_MS == 50U);
    test_compiled_defaults_are_applied();
    test_first_valid_sample_calculates_and_mixes();
    test_position_loop_updates_only_every_50_ms();
    test_yaw_error_wraps_across_boundary();
    test_yaw_error_wrap_does_not_spike_derivative();
    test_target_changes_take_effect_at_next_position_update();
    test_invalid_feedback_stops_and_recovery_restarts_pid();
    test_wheel_targets_scale_to_maximum_speed();
    test_invalid_debug_pid_stops_safely();
    return 0;
}
