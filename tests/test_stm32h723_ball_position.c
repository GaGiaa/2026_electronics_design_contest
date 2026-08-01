#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "app_ball_position_control.h"

static app_ball_position_control_config_t make_config(void)
{
    return (app_ball_position_control_config_t){
        .period_ms = APP_BALL_POSITION_CONTROL_PERIOD_MS,
        .max_age_ms = 100U,
        .safe_motor_position_deg = 134.0f,
        .pid_params = {
            .kp = 0.02f,
            .ki = 0.0f,
            .kd = 0.0f,
            .output_limit = 3.0f,
            .deadband = 1.0f,
        },
        .output_limit_deg = 3.0f,
        .sign = 1.0f,
        .use_hold_position_map = true,
        .deadband_mm = 1.0f,
        .hold_position_mm = {0.0f, 100.0f, 200.0f},
        .hold_motor_position_deg = {120.0f, 134.0f, 148.0f},
        .engage_error_mm = 6.0f,
        .release_error_mm = 2.0f,
        .breakaway_enable = true,
        .breakaway_pulse_deg = 1.0f,
        .breakaway_stall_time_ms = 200U,
        .breakaway_min_motion_mm = 1.0f,
        .breakaway_duration_ms = 60U,
        .breakaway_cooldown_ms = 500U,
    };
}

static app_ball_position_control_input_t make_input(uint32_t now_ms)
{
    return (app_ball_position_control_input_t){
        .now_ms = now_ms,
        .enabled = true,
        .target_mm = 120.0f,
        .measured_mm = 100.0f,
        .vision_valid = true,
        .vision_age_ms = 0U,
        .id3_ready = true,
    };
}

static void test_positive_error_requests_positive_motor_offset(void)
{
    app_ball_position_control_t control;
    const app_ball_position_control_config_t config = make_config();
    const app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;

    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);

    assert(output.state == APP_BALL_POSITION_STATE_ACTIVE);
    assert(output.fault == APP_BALL_POSITION_FAULT_NONE);
    assert(output.update_due);
    assert(output.valid);
    assert(fabsf(output.error_mm - 20.0f) < 0.0001f);
    assert(fabsf(output.pid_offset_deg - 0.4f) < 0.0001f);
    assert(fabsf(output.hold_motor_position_deg - 134.0f) < 0.0001f);
    assert(fabsf(output.target_motor_position_deg - 134.4f) < 0.0001f);
}

static void test_hold_map_interpolates_motor_position(void)
{
    app_ball_position_control_t control;
    const app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;

    input.target_mm = 150.0f;
    input.measured_mm = 150.0f;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);

    assert(!output.drive_active);
    assert(fabsf(output.hold_motor_position_deg - 141.0f) < 0.0001f);
    assert(fabsf(output.target_motor_position_deg - 141.0f) < 0.0001f);
}

static void test_fixed_base_ignores_hold_map(void)
{
    app_ball_position_control_t control;
    app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;

    config.use_hold_position_map = false;
    config.safe_motor_position_deg = 134.0f;
    input.target_mm = 150.0f;
    input.measured_mm = 150.0f;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);

    assert(!output.drive_active);
    assert(fabsf(output.hold_motor_position_deg - 134.0f) < 0.0001f);
    assert(fabsf(output.target_motor_position_deg - 134.0f) < 0.0001f);
}

static void test_updates_at_50_hz_only(void)
{
    app_ball_position_control_t control;
    const app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;
    float initial_target;

    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);
    initial_target = output.target_motor_position_deg;

    input.now_ms = 19U;
    input.target_mm = 220.0f;
    app_ball_position_control_step(&control, &input, &output);
    assert(!output.update_due);
    assert(fabsf(output.target_motor_position_deg - initial_target) < 0.0001f);

    input.now_ms = 20U;
    app_ball_position_control_step(&control, &input, &output);
    assert(output.update_due);
    assert(fabsf(output.pid_offset_deg - 2.4f) < 0.0001f);
}

static void test_deadband_and_offset_limit(void)
{
    app_ball_position_control_t control;
    app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;

    input.target_mm = 100.5f;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);
    assert(fabsf(output.pid_offset_deg) < 0.0001f);
    assert(fabsf(output.target_motor_position_deg - 134.0f) < 0.0001f);

    config.pid_params.kp = 1.0f;
    input.now_ms = 0U;
    input.target_mm = 200.0f;
    input.measured_mm = 0.0f;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);
    assert(fabsf(output.pid_offset_deg - 3.0f) < 0.0001f);
    assert(fabsf(output.target_motor_position_deg - 123.0f) < 0.0001f);
}

static void test_integral_freezes_inside_engage_release_hysteresis_band(void)
{
    app_ball_position_control_t control;
    app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;
    float integral_before_hysteresis;

    config.pid_params.kp = 0.0f;
    config.pid_params.ki = 1.0f;
    config.breakaway_enable = false;
    app_ball_position_control_init(&control, &config);

    app_ball_position_control_step(&control, &input, &output);
    integral_before_hysteresis = output.integral;
    assert(output.drive_active);
    assert(integral_before_hysteresis > 0.0f);

    input.now_ms = 20U;
    input.measured_mm = 116.0f; /* 4 mm error: inside the 2..6 mm hysteresis band. */
    app_ball_position_control_step(&control, &input, &output);

    assert(output.drive_active);
    assert(fabsf(output.integral - integral_before_hysteresis) < 0.0001f);
}

static void test_breakaway_pulse_triggers_after_stall_and_expires(void)
{
    app_ball_position_control_t control;
    app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;

    config.pid_params.kp = 0.0f;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);
    assert(!output.breakaway_active);
    for (input.now_ms = 20U; input.now_ms <= 180U; input.now_ms += 20U) {
        app_ball_position_control_step(&control, &input, &output);
        assert(!output.breakaway_active);
    }
    input.now_ms = 200U;
    app_ball_position_control_step(&control, &input, &output);
    assert(output.breakaway_active);
    assert(output.breakaway_trigger_count == 1U);
    assert(fabsf(output.breakaway_offset_deg - 1.0f) < 0.0001f);
    assert(fabsf(output.pid_offset_deg - 1.0f) < 0.0001f);

    input.now_ms = 220U;
    app_ball_position_control_step(&control, &input, &output);
    assert(output.breakaway_active);
    input.now_ms = 240U;
    app_ball_position_control_step(&control, &input, &output);
    assert(output.breakaway_active);
    input.now_ms = 260U;
    app_ball_position_control_step(&control, &input, &output);
    assert(!output.breakaway_active);
    assert(output.breakaway_trigger_count == 1U);
    assert(fabsf(output.pid_offset_deg) < 0.0001f);

    input.now_ms = 400U;
    app_ball_position_control_step(&control, &input, &output);
    assert(!output.breakaway_active);
    assert(output.breakaway_trigger_count == 1U);
}

static void test_breakaway_pulse_follows_position_sign_and_motion_resets_stall(void)
{
    app_ball_position_control_t control;
    app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;

    config.pid_params.kp = 0.0f;
    config.sign = -1.0f;
    input.target_mm = 80.0f;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);
    input.now_ms = 20U;
    input.measured_mm = 102.0f;
    app_ball_position_control_step(&control, &input, &output);
    assert(output.breakaway_stall_elapsed_ms == 0U);
    for (input.now_ms = 40U; input.now_ms <= 220U; input.now_ms += 20U) {
        app_ball_position_control_step(&control, &input, &output);
    }
    assert(output.breakaway_active);
    assert(fabsf(output.breakaway_offset_deg - 1.0f) < 0.0001f);
}

static void test_breakaway_resets_on_invalid_vision_without_integral_change(void)
{
    app_ball_position_control_t control;
    app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;
    float integral;

    config.pid_params.kp = 0.0f;
    config.pid_params.ki = 0.2f;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);
    integral = output.integral;
    input.now_ms = 20U;
    input.vision_valid = false;
    app_ball_position_control_step(&control, &input, &output);
    assert(output.reset);
    assert(!output.breakaway_active);
    assert(output.breakaway_stall_elapsed_ms == 0U);
    assert(fabsf(output.integral) < 0.0001f);
    assert(fabsf(integral) > 0.0f);
}

static void test_stale_vision_resets_pid_and_holds_last_safe_position(void)
{
    app_ball_position_control_t control;
    app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;
    float last_target;

    config.pid_params.kp = 0.0f;
    config.pid_params.ki = 1.0f;
    input.target_mm = 110.0f;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);
    assert(output.integral > 0.0f);
    last_target = output.target_motor_position_deg;

    input.now_ms = 20U;
    input.vision_age_ms = 101U;
    app_ball_position_control_step(&control, &input, &output);
    assert(output.state == APP_BALL_POSITION_STATE_HOLD);
    assert(output.fault == APP_BALL_POSITION_FAULT_VISION_STALE);
    assert(output.reset);
    assert(fabsf(output.integral) < 0.0001f);
    assert(fabsf(output.target_motor_position_deg - last_target) < 0.0001f);

    input.now_ms = 40U;
    input.vision_age_ms = 0U;
    app_ball_position_control_step(&control, &input, &output);
    assert(output.state == APP_BALL_POSITION_STATE_ACTIVE);
    assert(output.update_due);
    assert(output.integral > 0.0f);
}

static void test_first_fault_uses_safe_motor_position(void)
{
    app_ball_position_control_t control;
    const app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;

    input.vision_valid = false;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);

    assert(output.state == APP_BALL_POSITION_STATE_HOLD);
    assert(fabsf(output.target_motor_position_deg - 134.0f) < 0.0001f);
}

static void test_invalid_configuration_faults(void)
{
    app_ball_position_control_t control;
    app_ball_position_control_config_t config = make_config();

    config.pid_params.kp = NAN;
    app_ball_position_control_init(&control, &config);
    assert(control.state == APP_BALL_POSITION_STATE_FAULT);
    assert(control.fault == APP_BALL_POSITION_FAULT_INVALID_CONFIG);

    config = make_config();
    config.period_ms = 19U;
    app_ball_position_control_init(&control, &config);
    assert(control.state == APP_BALL_POSITION_STATE_FAULT);
    assert(control.fault == APP_BALL_POSITION_FAULT_INVALID_CONFIG);

    config = make_config();
    config.hold_motor_position_deg[1] = NAN;
    app_ball_position_control_init(&control, &config);
    assert(control.state == APP_BALL_POSITION_STATE_FAULT);
    assert(control.fault == APP_BALL_POSITION_FAULT_INVALID_CONFIG);

    config = make_config();
    config.breakaway_pulse_deg = NAN;
    app_ball_position_control_init(&control, &config);
    assert(control.state == APP_BALL_POSITION_STATE_FAULT);
    assert(control.fault == APP_BALL_POSITION_FAULT_INVALID_CONFIG);
}

static void test_derivative_on_measurement_ignores_target_step(void)
{
    app_ball_position_control_t control;
    app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;

    config.pid_params.kp = 0.0f;
    config.pid_params.kd = 0.1f;
    config.output_limit_deg = 100.0f;
    config.pid_params.output_limit = 100.0f;
    config.breakaway_enable = false;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);

    input.now_ms = 20U;
    input.target_mm = 140.0f;
    app_ball_position_control_step(&control, &input, &output);

    assert(fabsf(output.d_out_deg) < 0.0001f);
}

static void test_derivative_on_measurement_follows_feedback_change(void)
{
    app_ball_position_control_t control;
    app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;

    config.pid_params.kp = 0.0f;
    config.pid_params.kd = 0.1f;
    config.output_limit_deg = 100.0f;
    config.pid_params.output_limit = 100.0f;
    config.breakaway_enable = false;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);

    input.now_ms = 20U;
    input.measured_mm = 110.0f;
    app_ball_position_control_step(&control, &input, &output);

    assert(fabsf(output.d_out_deg + 50.0f) < 0.0001f);
}

static void test_derivative_history_clears_after_invalid_vision(void)
{
    app_ball_position_control_t control;
    app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;

    config.pid_params.kp = 0.0f;
    config.pid_params.kd = 0.1f;
    config.output_limit_deg = 100.0f;
    config.pid_params.output_limit = 100.0f;
    config.breakaway_enable = false;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);

    input.now_ms = 20U;
    input.vision_valid = false;
    app_ball_position_control_step(&control, &input, &output);

    input.now_ms = 40U;
    input.vision_valid = true;
    input.measured_mm = 110.0f;
    app_ball_position_control_step(&control, &input, &output);

    assert(fabsf(output.d_out_deg) < 0.0001f);
}

static void test_default_configuration_matches_tuned_watch_values(void)
{
    app_ball_position_control_config_t config;

    app_ball_position_control_config_default(&config);

    assert(fabsf(config.pid_params.kp - 2.0f) < 0.0001f);
    assert(fabsf(config.pid_params.ki) < 0.0001f);
    assert(fabsf(config.pid_params.kd - 0.8f) < 0.0001f);
    assert(fabsf(config.output_limit_deg - 360.0f) < 0.0001f);
    assert(fabsf(config.deadband_mm - 0.5f) < 0.0001f);
    assert(fabsf(config.engage_error_mm - 0.5f) < 0.0001f);
    assert(fabsf(config.release_error_mm - 0.5f) < 0.0001f);
    assert(!config.breakaway_enable);
    assert(fabsf(config.breakaway_pulse_deg - 10.0f) < 0.0001f);
    assert(config.breakaway_stall_time_ms == 200U);
    assert(fabsf(config.breakaway_min_motion_mm - 1.0f) < 0.0001f);
    assert(config.breakaway_duration_ms == 60U);
    assert(config.breakaway_cooldown_ms == 500U);
}

int main(void)
{
    test_positive_error_requests_positive_motor_offset();
    test_hold_map_interpolates_motor_position();
    test_fixed_base_ignores_hold_map();
    test_updates_at_50_hz_only();
    test_deadband_and_offset_limit();
    test_integral_freezes_inside_engage_release_hysteresis_band();
    test_breakaway_pulse_triggers_after_stall_and_expires();
    test_breakaway_pulse_follows_position_sign_and_motion_resets_stall();
    test_breakaway_resets_on_invalid_vision_without_integral_change();
    test_stale_vision_resets_pid_and_holds_last_safe_position();
    test_first_fault_uses_safe_motor_position();
    test_invalid_configuration_faults();
    test_derivative_on_measurement_ignores_target_step();
    test_derivative_on_measurement_follows_feedback_change();
    test_derivative_history_clears_after_invalid_vision();
    test_default_configuration_matches_tuned_watch_values();
    return 0;
}
