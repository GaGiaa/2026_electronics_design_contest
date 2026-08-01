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
        .pid_params = {
            .kp = 0.02f,
            .ki = 0.0f,
            .kd = 0.0f,
            .output_limit = 3.0f,
            .deadband = 1.0f,
        },
        .output_limit_deg = 3.0f,
        .sign = -1.0f,
        .deadband_mm = 1.0f,
        .hold_position_mm = {0.0f, 100.0f, 200.0f},
        .hold_tilt_deg = {0.0f, 0.0f, 0.0f},
        .engage_error_mm = 6.0f,
        .release_error_mm = 2.0f,
        .breakaway_positive_tilt_deg = 0.0f,
        .breakaway_negative_tilt_deg = 0.0f,
        .velocity_gain_deg_per_mm_s = 0.0f,
        .velocity_filter_alpha = 0.35f,
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
        .vision_frame_count = 1U,
        .vision_sample_ms = now_ms,
        .calibration_ready = true,
        .id3_ready = true,
    };
}

static void test_positive_position_error_requests_negative_tilt(void)
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
    assert(fabsf(output.p_out_deg - 0.4f) < 0.0001f);
    assert(fabsf(output.target_tilt_deg + 0.4f) < 0.0001f);
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
    initial_target = output.target_tilt_deg;

    input.now_ms = 24U;
    input.target_mm = 220.0f;
    app_ball_position_control_step(&control, &input, &output);
    assert(!output.update_due);
    assert(fabsf(output.target_tilt_deg - initial_target) < 0.0001f);

    input.now_ms = 25U;
    app_ball_position_control_step(&control, &input, &output);
    assert(output.update_due);
    assert(fabsf(output.target_tilt_deg + 2.4f) < 0.0001f);
}

static void test_deadband_and_output_limit(void)
{
    app_ball_position_control_t control;
    app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;

    input.target_mm = 100.5f;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);
    assert(fabsf(output.error_mm) < 0.0001f);
    assert(fabsf(output.target_tilt_deg) < 0.0001f);

    config.pid_params.kp = 1.0f;
    input.now_ms = 0U;
    input.target_mm = 200.0f;
    input.measured_mm = 0.0f;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);
    assert(fabsf(output.target_tilt_deg + 3.0f) < 0.0001f);
}

static void test_stale_vision_resets_pid_and_recovers(void)
{
    app_ball_position_control_t control;
    app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;

    config.pid_params.kp = 0.0f;
    config.pid_params.ki = 1.0f;
    input.target_mm = 110.0f;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);
    assert(output.integral > 0.0f);

    input.now_ms = 25U;
    input.vision_age_ms = 101U;
    app_ball_position_control_step(&control, &input, &output);
    assert(output.state == APP_BALL_POSITION_STATE_HOLD);
    assert(output.fault == APP_BALL_POSITION_FAULT_VISION_STALE);
    assert(output.reset);
    assert(fabsf(output.integral) < 0.0001f);
    assert(fabsf(output.target_tilt_deg) < 0.0001f);

    input.now_ms = 50U;
    input.vision_age_ms = 0U;
    app_ball_position_control_step(&control, &input, &output);
    assert(output.state == APP_BALL_POSITION_STATE_ACTIVE);
    assert(output.update_due);
    assert(output.integral > 0.0f);
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
    config.engage_error_mm = 1.0f;
    config.release_error_mm = 2.0f;
    app_ball_position_control_init(&control, &config);
    assert(control.state == APP_BALL_POSITION_STATE_FAULT);
    assert(control.fault == APP_BALL_POSITION_FAULT_INVALID_CONFIG);
}

static void test_hold_map_and_breakaway_hysteresis(void)
{
    app_ball_position_control_t control;
    app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;

    config.hold_tilt_deg[1] = 0.20f;
    config.hold_tilt_deg[2] = 0.40f;
    config.breakaway_negative_tilt_deg = 0.15f;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);
    assert(output.drive_active);
    assert(fabsf(output.hold_tilt_deg - 0.20f) < 0.0001f);
    assert(fabsf(output.breakaway_tilt_deg + 0.15f) < 0.0001f);
    assert(fabsf(output.target_tilt_deg + 0.35f) < 0.0001f);

    input.now_ms = 25U;
    input.target_mm = 102.0f;
    input.vision_frame_count = 2U;
    input.vision_sample_ms = 25U;
    app_ball_position_control_step(&control, &input, &output);
    assert(!output.drive_active);
    assert(fabsf(output.output_deg) < 0.0001f);
    assert(fabsf(output.target_tilt_deg - 0.20f) < 0.0001f);
}

static void test_velocity_damping_uses_new_vision_frames(void)
{
    app_ball_position_control_t control;
    app_ball_position_control_config_t config = make_config();
    app_ball_position_control_input_t input = make_input(0U);
    app_ball_position_control_output_t output;

    config.pid_params.kp = 0.0f;
    config.velocity_gain_deg_per_mm_s = 0.002f;
    config.velocity_filter_alpha = 0.5f;
    input.target_mm = 100.0f;
    app_ball_position_control_init(&control, &config);
    app_ball_position_control_step(&control, &input, &output);
    assert(fabsf(output.velocity_mm_s) < 0.0001f);

    input.now_ms = 25U;
    input.measured_mm = 110.0f;
    input.vision_frame_count = 2U;
    input.vision_sample_ms = 25U;
    app_ball_position_control_step(&control, &input, &output);
    assert(fabsf(output.velocity_mm_s - 200.0f) < 0.0001f);
    assert(fabsf(output.velocity_damping_tilt_deg - 0.40f) < 0.0001f);
    assert(fabsf(output.target_tilt_deg - 0.40f) < 0.0001f);
}

int main(void)
{
    test_positive_position_error_requests_negative_tilt();
    test_updates_at_40_hz_only();
    test_deadband_and_output_limit();
    test_stale_vision_resets_pid_and_recovers();
    test_invalid_configuration_faults();
    test_hold_map_and_breakaway_hysteresis();
    test_velocity_damping_uses_new_vision_frames();
    return 0;
}
