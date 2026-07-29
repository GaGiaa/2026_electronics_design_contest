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

static PID_Position_Param_Config make_position_params(void)
{
    PID_Position_Param_Config params = {0};
    params.kp = 1.0f;
    params.ki = 0.0f;
    params.kd = 0.0f;
    params.output_limit = 1000.0f;
    params.deadband = 0.0f;
    return params;
}

static void test_selected_id_uses_macro_default_and_watch_override(void)
{
    assert(app_single_motor_sanitize_id(0U, 2U) == 2U);
    assert(app_single_motor_sanitize_id(4U, 2U) == 2U);
    assert(app_single_motor_sanitize_id(3U, 2U) == 3U);
}

static void test_runtime_speed_limit_sanitization(void)
{
    assert(fabsf(app_single_motor_sanitize_max_output_speed_rpm(40.0f) - 40.0f) < 0.0001f);
    assert(fabsf(app_single_motor_sanitize_max_output_speed_rpm(10000.0f) - 10000.0f) < 0.0001f);
    assert(app_single_motor_sanitize_max_output_speed_rpm(0.0f) == 0.0f);
    assert(app_single_motor_sanitize_max_output_speed_rpm(NAN) == 0.0f);
}

static void test_feedback_timeout_and_disable_force_zero(void)
{
    app_single_motor_step_input_t input = {0};
    app_single_motor_step_output_t output = {0};
    PID_Incremental speed_pid;
    PID_Position position_pid;
    PID_Incremental_Param_Config params = make_params();
    PID_Position_Param_Config position_params = make_position_params();

    input.enable = 1U;
    input.control_mode = APP_SINGLE_MOTOR_CONTROL_MODE_SPEED;
    input.selected_id = 2U;
    input.target_output_speed_rpm = 10.0f;
    input.params = params;
    input.feedback_valid = true;
    input.feedback_age_ms = 10U;
    input.feedback_output_speed_rpm = 0.0f;
    PID_Incremental_Init(&speed_pid, &params, 0.001f);
    PID_Position_Init(&position_pid, &position_params, 0.005f);

    assert(app_single_motor_step(&speed_pid, &position_pid, &input, 50U, 83.333333f, 10.0f, &output));
    assert(output.selected_id == 2U);
    assert(output.commanded_current_A == 10.0f);
    assert(fabsf(output.raw_output_A - 10.0f) < 0.001f);

    input.feedback_age_ms = 50U;
    assert(!app_single_motor_step(&speed_pid, &position_pid, &input, 50U, 3000.0f, 3000.0f, &output));
    assert(output.commanded_current_A == 0.0f);
    assert(output.reset_pid);

    input.feedback_age_ms = 0U;
    input.enable = 0U;
    assert(!app_single_motor_step(&speed_pid, &position_pid, &input, 50U, 3000.0f, 3000.0f, &output));
    assert(output.commanded_current_A == 0.0f);
}

static void test_target_speed_and_parameters_are_rejected_when_invalid(void)
{
    app_single_motor_step_input_t input = {0};
    app_single_motor_step_output_t output = {0};
    PID_Incremental speed_pid;
    PID_Position position_pid;
    PID_Incremental_Param_Config params = make_params();
    PID_Position_Param_Config position_params = make_position_params();

    input.enable = 1U;
    input.control_mode = APP_SINGLE_MOTOR_CONTROL_MODE_SPEED;
    input.selected_id = 1U;
    input.target_output_speed_rpm = 100.0f;
    input.params = params;
    input.feedback_valid = true;
    input.feedback_output_speed_rpm = 0.0f;
    PID_Incremental_Init(&speed_pid, &params, 0.001f);
    PID_Position_Init(&position_pid, &position_params, 0.005f);
    assert(!app_single_motor_step(&speed_pid, &position_pid, &input, 50U, 83.333333f, 10.0f, &output));
    assert(output.commanded_current_A == 0.0f);
    assert(output.reset_pid);

    input.target_output_speed_rpm = 10.0f;
    input.params.kp = NAN;
    assert(!app_single_motor_step(&speed_pid, &position_pid, &input, 50U, 83.333333f, 10.0f, &output));
    assert(output.commanded_current_A == 0.0f);
}

static void test_position_mode_updates_outer_loop_every_five_ms(void)
{
    app_single_motor_step_input_t input = {0};
    app_single_motor_step_output_t output = {0};
    PID_Incremental speed_pid;
    PID_Position position_pid;
    PID_Incremental_Param_Config speed_params = make_params();
    PID_Position_Param_Config position_params = make_position_params();

    input.enable = 1U;
    input.control_mode = APP_SINGLE_MOTOR_CONTROL_MODE_POSITION;
    input.selected_id = 1U;
    input.target_output_speed_rpm = NAN;
    input.target_position_deg = 10.0f;
    input.params = speed_params;
    input.position_params = position_params;
    input.position_reference_valid = true;
    input.position_update_due = false;
    input.feedback_valid = true;
    input.feedback_output_speed_rpm = 0.0f;
    input.feedback_position_deg = 0.0f;
    PID_Incremental_Init(&speed_pid, &speed_params, 0.001f);
    PID_Position_Init(&position_pid, &position_params, 0.005f);

    assert(app_single_motor_step(&speed_pid, &position_pid, &input, 50U, 100.0f, 10.0f, &output));
    assert(output.target_output_speed_rpm == 0.0f);
    assert(output.commanded_current_A == 0.0f);

    input.position_update_due = true;
    assert(app_single_motor_step(&speed_pid, &position_pid, &input, 50U, 100.0f, 10.0f, &output));
    assert(output.position_output_rpm == 10.0f);
    assert(output.target_output_speed_rpm == 10.0f);
    assert(output.commanded_current_A == 10.0f);

    input.target_position_deg = 20.0f;
    input.position_update_due = false;
    assert(app_single_motor_step(&speed_pid, &position_pid, &input, 50U, 100.0f, 10.0f, &output));
    assert(output.target_output_speed_rpm == 10.0f);

    input.position_update_due = true;
    assert(app_single_motor_step(&speed_pid, &position_pid, &input, 50U, 100.0f, 10.0f, &output));
    assert(output.target_output_speed_rpm == 20.0f);

    input.position_reference_valid = false;
    assert(!app_single_motor_step(&speed_pid, &position_pid, &input, 50U, 100.0f, 10.0f, &output));
    assert(output.commanded_current_A == 0.0f);
    assert(output.reset_pid);
}

static void test_position_mode_rejects_nonfinite_pid_state(void)
{
    app_single_motor_step_input_t input = {0};
    app_single_motor_step_output_t output = {0};
    PID_Incremental speed_pid;
    PID_Position position_pid;
    PID_Incremental_Param_Config speed_params = make_params();
    PID_Position_Param_Config position_params = make_position_params();

    input.enable = 1U;
    input.control_mode = APP_SINGLE_MOTOR_CONTROL_MODE_POSITION;
    input.selected_id = 1U;
    input.params = speed_params;
    input.position_params = position_params;
    input.target_position_deg = 1.0f;
    input.feedback_position_deg = 0.0f;
    input.position_reference_valid = true;
    input.feedback_valid = true;
    PID_Incremental_Init(&speed_pid, &speed_params, 0.001f);
    PID_Position_Init(&position_pid, &position_params, 0.005f);
    position_pid.integral = NAN;

    assert(!app_single_motor_step(&speed_pid, &position_pid, &input, 50U, 100.0f, 10.0f,
                                  &output));
    assert(output.commanded_current_A == 0.0f);
    assert(output.reset_pid);
    assert(position_pid.integral == 0.0f);
}

int main(void)
{
    test_selected_id_uses_macro_default_and_watch_override();
    test_runtime_speed_limit_sanitization();
    test_feedback_timeout_and_disable_force_zero();
    test_target_speed_and_parameters_are_rejected_when_invalid();
    test_position_mode_updates_outer_loop_every_five_ms();
    test_position_mode_rejects_nonfinite_pid_state();
    return 0;
}
