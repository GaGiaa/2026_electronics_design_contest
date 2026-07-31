#include <assert.h>
#include <math.h>
#include <stdbool.h>

#include "app_tilt_control.h"

static app_tilt_control_config_t make_config(void)
{
    return (app_tilt_control_config_t){
        .position_min_deg = 80.0f,
        .position_max_deg = 180.0f,
        .sample_period_ms = 5U,
        .sample_max_age_ms = 30U,
        .pitch_to_tilt_sign = -1.0f,
        .derivative_filter_N = 20.0f,
        .pid_params = {
            .kp = 6.0f,
            .ki = 0.0f,
            .kd = 0.0f,
            .output_limit = 10.0f,
            .deadband = 0.20f,
        },
    };
}

static app_tilt_control_input_t make_input(uint32_t now_ms, uint32_t sample_count)
{
    return (app_tilt_control_input_t){
        .now_ms = now_ms,
        .enabled = true,
        .target_tilt_deg = 0.0f,
        .capture_zero_request = true,
        .balance_zero_valid = true,
        .motor_feedback_valid = true,
        .motor_feedback_position_deg = 120.0f,
        .imu_online = true,
        .imu_valid = true,
        .imu_pitch_deg = 0.0f,
        .imu_sample_count = sample_count,
        .imu_sample_age_ms = 0U,
    };
}

static void test_enable_before_zero_capture_holds_motor_position(void)
{
    app_tilt_control_t control;
    app_tilt_control_config_t config = make_config();
    app_tilt_control_input_t input = make_input(0U, 1U);
    app_tilt_control_output_t output;

    input.capture_zero_request = false;
    input.imu_pitch_deg = 3.25f;
    app_tilt_control_init(&control, &config);
    app_tilt_control_step(&control, &input, &output);
    assert(output.state == APP_TILT_STATE_HOLD);
    assert(output.fault == APP_TILT_FAULT_ZERO_NOT_CAPTURED);
    assert(!output.zero_captured_valid);
    assert(fabsf(output.motor_target_position_deg - 120.0f) < 0.0001f);
}

static void test_positive_pitch_maps_to_negative_tilt_and_decreases_motor_target(void)
{
    app_tilt_control_t control;
    app_tilt_control_config_t config = make_config();
    app_tilt_control_input_t input = make_input(0U, 1U);
    app_tilt_control_output_t output;

    app_tilt_control_init(&control, &config);
    app_tilt_control_step(&control, &input, &output);
    assert(output.state == APP_TILT_STATE_ACTIVE);
    assert(fabsf(output.motor_target_position_deg - 120.0f) < 0.0001f);

    input.now_ms = 5U;
    input.imu_sample_count = 2U;
    input.imu_pitch_deg = 1.0f;
    app_tilt_control_step(&control, &input, &output);
    assert(fabsf(output.tilt_deg + 1.0f) < 0.0001f);
    assert(output.error_deg > 0.0f);
    assert(output.pid_rate_deg_s > 0.0f);
    assert(fabsf(output.motor_target_position_deg - 119.97f) < 0.0001f);
}

static void test_capture_zero_makes_current_pitch_the_zero_tilt_reference(void)
{
    app_tilt_control_t control;
    app_tilt_control_config_t config = make_config();
    app_tilt_control_input_t input = make_input(0U, 1U);
    app_tilt_control_output_t output;

    input.capture_zero_request = true;
    input.imu_pitch_deg = 3.25f;
    app_tilt_control_init(&control, &config);
    app_tilt_control_step(&control, &input, &output);
    assert(output.capture_zero_consumed);
    assert(fabsf(output.captured_zero_deg - 3.25f) < 0.0001f);
    assert(fabsf(output.tilt_deg) < 0.0001f);
}

static void test_repeated_sample_does_not_advance_motor_target(void)
{
    app_tilt_control_t control;
    app_tilt_control_config_t config = make_config();
    app_tilt_control_input_t input = make_input(0U, 1U);
    app_tilt_control_output_t output;
    float target_after_new_sample;

    app_tilt_control_init(&control, &config);
    app_tilt_control_step(&control, &input, &output);
    input.now_ms = 5U;
    input.imu_sample_count = 2U;
    input.imu_pitch_deg = 1.0f;
    app_tilt_control_step(&control, &input, &output);
    target_after_new_sample = output.motor_target_position_deg;

    input.now_ms = 11U;
    app_tilt_control_step(&control, &input, &output);
    assert(!output.new_imu_sample);
    assert(fabsf(output.motor_target_position_deg - target_after_new_sample) < 0.0001f);
}

static void test_stale_imu_holds_position_and_resets_pid(void)
{
    app_tilt_control_t control;
    app_tilt_control_config_t config = make_config();
    app_tilt_control_input_t input = make_input(0U, 1U);
    app_tilt_control_output_t output;
    float target_before_timeout;

    app_tilt_control_init(&control, &config);
    app_tilt_control_step(&control, &input, &output);
    input.now_ms = 5U;
    input.imu_sample_count = 2U;
    input.imu_pitch_deg = -1.0f;
    app_tilt_control_step(&control, &input, &output);
    target_before_timeout = output.motor_target_position_deg;

    input.now_ms = 35U;
    input.imu_sample_age_ms = 31U;
    app_tilt_control_step(&control, &input, &output);
    assert(output.state == APP_TILT_STATE_HOLD);
    assert(output.fault == APP_TILT_FAULT_IMU_STALE);
    assert(fabsf(output.motor_target_position_deg - target_before_timeout) < 0.0001f);
    assert(fabsf(output.pid_integral) < 0.0001f);
}

static void test_position_target_clamps_to_safe_range(void)
{
    app_tilt_control_t control;
    app_tilt_control_config_t config = make_config();
    app_tilt_control_input_t input = make_input(0U, 1U);
    app_tilt_control_output_t output;

    input.motor_feedback_position_deg = 80.0f;
    app_tilt_control_init(&control, &config);
    app_tilt_control_step(&control, &input, &output);
    input.now_ms = 5U;
    input.imu_sample_count = 2U;
    input.imu_pitch_deg = 10.0f;
    app_tilt_control_step(&control, &input, &output);
    assert(output.motor_target_clamped);
    assert(fabsf(output.motor_target_position_deg - 80.0f) < 0.0001f);
}

static void test_target_step_does_not_cause_derivative_kick(void)
{
    app_tilt_control_t control;
    app_tilt_control_config_t config = make_config();
    app_tilt_control_input_t input = make_input(0U, 1U);
    app_tilt_control_output_t output;

    config.pid_params.kp = 0.0f;
    config.pid_params.ki = 0.0f;
    config.pid_params.kd = 2.0f;
    config.pid_params.output_limit = 1000.0f;
    app_tilt_control_init(&control, &config);
    app_tilt_control_step(&control, &input, &output);

    input.now_ms = 5U;
    input.imu_sample_count = 2U;
    input.target_tilt_deg = 1.0f;
    app_tilt_control_step(&control, &input, &output);
    assert(fabsf(output.pid_d_out_deg_s) < 0.0001f);
    assert(fabsf(output.pid_rate_deg_s) < 0.0001f);
}

static void test_measurement_derivative_is_filtered_and_opposes_rising_tilt(void)
{
    app_tilt_control_t control;
    app_tilt_control_config_t config = make_config();
    app_tilt_control_input_t input = make_input(0U, 1U);
    app_tilt_control_output_t output;

    config.pid_params.kp = 0.0f;
    config.pid_params.ki = 0.0f;
    config.pid_params.kd = 2.0f;
    config.pid_params.output_limit = 1000.0f;
    app_tilt_control_init(&control, &config);
    app_tilt_control_step(&control, &input, &output);

    input.now_ms = 5U;
    input.imu_sample_count = 2U;
    input.imu_pitch_deg = -1.0f;
    app_tilt_control_step(&control, &input, &output);
    assert(fabsf(output.measured_tilt_rate_deg_s - 200.0f) < 0.001f);
    assert(fabsf(output.filtered_tilt_rate_deg_s - 18.181818f) < 0.001f);
    assert(fabsf(output.pid_d_out_deg_s + 36.363636f) < 0.001f);
}

static void test_outward_position_limit_prevents_integral_windup(void)
{
    app_tilt_control_t control;
    app_tilt_control_config_t config = make_config();
    app_tilt_control_input_t input = make_input(0U, 1U);
    app_tilt_control_output_t output;

    config.pid_params.kp = 0.0f;
    config.pid_params.ki = 100.0f;
    config.pid_params.kd = 0.0f;
    config.pid_params.output_limit = 1000.0f;
    input.motor_feedback_position_deg = 80.0f;
    app_tilt_control_init(&control, &config);
    app_tilt_control_step(&control, &input, &output);

    input.now_ms = 5U;
    input.imu_sample_count = 2U;
    input.imu_pitch_deg = 1.0f;
    app_tilt_control_step(&control, &input, &output);
    assert(output.motor_target_clamped);
    assert(fabsf(output.pid_integral) < 0.0001f);
    assert(fabsf(output.pid_i_out_deg_s) < 0.0001f);
}

static void test_rate_and_position_limits_do_not_reverse_integral(void)
{
    app_tilt_control_t control;
    app_tilt_control_config_t config = make_config();
    app_tilt_control_input_t input = make_input(0U, 1U);
    app_tilt_control_output_t output;

    config.pid_params.kp = 100.0f;
    config.pid_params.ki = 100.0f;
    config.pid_params.kd = 0.0f;
    config.pid_params.output_limit = 0.25f;
    input.motor_feedback_position_deg = 80.0f;
    app_tilt_control_init(&control, &config);
    app_tilt_control_step(&control, &input, &output);

    input.now_ms = 5U;
    input.imu_sample_count = 2U;
    input.imu_pitch_deg = 1.0f;
    app_tilt_control_step(&control, &input, &output);
    assert(fabsf(output.motor_target_position_deg - 80.0f) < 0.0001f);
    assert(fabsf(output.pid_integral) < 0.0001f);
}

static void test_capture_zero_clears_derivative_history(void)
{
    app_tilt_control_t control;
    app_tilt_control_config_t config = make_config();
    app_tilt_control_input_t input = make_input(0U, 1U);
    app_tilt_control_output_t output;

    config.pid_params.kp = 0.0f;
    config.pid_params.kd = 2.0f;
    app_tilt_control_init(&control, &config);
    app_tilt_control_step(&control, &input, &output);

    input.now_ms = 5U;
    input.imu_sample_count = 2U;
    input.imu_pitch_deg = -1.0f;
    app_tilt_control_step(&control, &input, &output);
    assert(output.pid_d_out_deg_s < 0.0f);

    input.now_ms = 10U;
    input.imu_sample_count = 3U;
    input.capture_zero_request = false;
    app_tilt_control_step(&control, &input, &output);

    input.now_ms = 15U;
    input.imu_sample_count = 4U;
    input.capture_zero_request = true;
    app_tilt_control_step(&control, &input, &output);
    assert(output.capture_zero_consumed);
    assert(fabsf(output.pid_d_out_deg_s) < 0.0001f);
}

static void test_invalid_derivative_filter_configuration_faults(void)
{
    app_tilt_control_t control;
    app_tilt_control_config_t config = make_config();

    config.derivative_filter_N = NAN;
    app_tilt_control_init(&control, &config);
    assert(control.state == APP_TILT_STATE_FAULT);
    assert(control.fault == APP_TILT_FAULT_INVALID_CONFIG);
}

int main(void)
{
    test_enable_before_zero_capture_holds_motor_position();
    test_positive_pitch_maps_to_negative_tilt_and_decreases_motor_target();
    test_capture_zero_makes_current_pitch_the_zero_tilt_reference();
    test_repeated_sample_does_not_advance_motor_target();
    test_stale_imu_holds_position_and_resets_pid();
    test_position_target_clamps_to_safe_range();
    test_target_step_does_not_cause_derivative_kick();
    test_measurement_derivative_is_filtered_and_opposes_rising_tilt();
    test_outward_position_limit_prevents_integral_windup();
    test_rate_and_position_limits_do_not_reverse_integral();
    test_capture_zero_clears_derivative_history();
    test_invalid_derivative_filter_configuration_faults();
    return 0;
}
