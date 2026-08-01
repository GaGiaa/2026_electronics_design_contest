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
        .deadband_mm = 1.0f,
        .hold_position_mm = {0.0f, 100.0f, 200.0f},
        .hold_motor_position_deg = {120.0f, 134.0f, 148.0f},
        .engage_error_mm = 6.0f,
        .release_error_mm = 2.0f,
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

static void test_updates_at_40_hz_only(void)
{
    app_ball_position_control_t control;
    const app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;
    float initial_target;

    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);
    initial_target = output.target_motor_position_deg;

    input.now_ms = 24U;
    input.target_mm = 220.0f;
    app_ball_position_control_step(&control, &input, &output);
    assert(!output.update_due);
    assert(fabsf(output.target_motor_position_deg - initial_target) < 0.0001f);

    input.now_ms = 25U;
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

    input.now_ms = 25U;
    input.vision_age_ms = 101U;
    app_ball_position_control_step(&control, &input, &output);
    assert(output.state == APP_BALL_POSITION_STATE_HOLD);
    assert(output.fault == APP_BALL_POSITION_FAULT_VISION_STALE);
    assert(output.reset);
    assert(fabsf(output.integral) < 0.0001f);
    assert(fabsf(output.target_motor_position_deg - last_target) < 0.0001f);

    input.now_ms = 50U;
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
    config.period_ms = 20U;
    app_ball_position_control_init(&control, &config);
    assert(control.state == APP_BALL_POSITION_STATE_FAULT);
    assert(control.fault == APP_BALL_POSITION_FAULT_INVALID_CONFIG);

    config = make_config();
    config.hold_motor_position_deg[1] = NAN;
    app_ball_position_control_init(&control, &config);
    assert(control.state == APP_BALL_POSITION_STATE_FAULT);
    assert(control.fault == APP_BALL_POSITION_FAULT_INVALID_CONFIG);
}

int main(void)
{
    test_positive_error_requests_positive_motor_offset();
    test_hold_map_interpolates_motor_position();
    test_updates_at_40_hz_only();
    test_deadband_and_offset_limit();
    test_stale_vision_resets_pid_and_holds_last_safe_position();
    test_first_fault_uses_safe_motor_position();
    test_invalid_configuration_faults();
    return 0;
}
