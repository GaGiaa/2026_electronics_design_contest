#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "app_single_motor.h"

static PID_Incremental_Param_Config make_params(void)
{
    PID_Incremental_Param_Config params = {0};
    params.kp = 1.0f;
    params.ki = 0.0f;
    params.kd = 0.0f;
    params.output_limit = 1000.0f;
    params.deadband = 0.0f;
    params.integral_output_limit = 1000.0f;
    params.integral_separation_threshold = 0.0f;
    params.derivative_filter_N = 0.0f;
    params.output_delta_limit = 0.0f;
    return params;
}

static void test_selected_id_uses_macro_default_and_watch_override(void)
{
    assert(app_single_motor_sanitize_id(0U, 2U) == 2U);
    assert(app_single_motor_sanitize_id(4U, 2U) == 2U);
    assert(app_single_motor_sanitize_id(3U, 2U) == 3U);
}

static void test_feedback_timeout_and_disable_force_zero(void)
{
    app_single_motor_step_input_t input = {0};
    app_single_motor_step_output_t output = {0};
    PID_Incremental pid;
    PID_Incremental_Param_Config params = make_params();

    input.enable = 1U;
    input.selected_id = 2U;
    input.target_speed_rpm = 100.0f;
    input.params = params;
    input.feedback_valid = true;
    input.feedback_age_ms = 10U;
    input.feedback_speed_rpm = 0;
    PID_Incremental_Init(&pid, &params, 0.001f);

    assert(app_single_motor_step(&pid, &input, 50U, 3000.0f, 3000.0f, &output));
    assert(output.selected_id == 2U);
    assert(output.commanded_current == 100);
    assert(fabsf(output.raw_output - 100.0f) < 0.001f);

    input.feedback_age_ms = 50U;
    assert(!app_single_motor_step(&pid, &input, 50U, 3000.0f, 3000.0f, &output));
    assert(output.commanded_current == 0);
    assert(output.reset_pid);

    input.feedback_age_ms = 0U;
    input.enable = 0U;
    assert(!app_single_motor_step(&pid, &input, 50U, 3000.0f, 3000.0f, &output));
    assert(output.commanded_current == 0);
}

static void test_target_speed_and_parameters_are_rejected_when_invalid(void)
{
    app_single_motor_step_input_t input = {0};
    app_single_motor_step_output_t output = {0};
    PID_Incremental pid;
    PID_Incremental_Param_Config params = make_params();

    input.enable = 1U;
    input.selected_id = 1U;
    input.target_speed_rpm = 4000.0f;
    input.params = params;
    input.feedback_valid = true;
    input.feedback_speed_rpm = 0;
    PID_Incremental_Init(&pid, &params, 0.001f);
    assert(!app_single_motor_step(&pid, &input, 50U, 3000.0f, 3000.0f, &output));
    assert(output.commanded_current == 0);
    assert(output.reset_pid);

    input.target_speed_rpm = 100.0f;
    input.params.kp = NAN;
    assert(!app_single_motor_step(&pid, &input, 50U, 3000.0f, 3000.0f, &output));
    assert(output.commanded_current == 0);
}

int main(void)
{
    test_selected_id_uses_macro_default_and_watch_override();
    test_feedback_timeout_and_disable_force_zero();
    test_target_speed_and_parameters_are_rejected_when_invalid();
    return 0;
}
