#include <assert.h>
#include <math.h>
#include <stdbool.h>

#include "app_balance.h"

static app_balance_config_t make_config(void)
{
    app_balance_config_t config = {0};

    config.home_search_output_speed_rpm = -8.0f;
    config.home_current_limit_a = 0.35f;
    config.home_stall_speed_rpm = 1.0f;
    config.home_stall_current_ratio = 0.8f;
    config.home_confirm_ms = 300U;
    config.home_timeout_ms = 12000U;
    config.position_min_deg = 0.0f;
    config.position_active_min_deg = 0.0f;
    config.position_max_deg = 30.0f;
    config.position_debug_active_min_deg = 1.0f;
    config.position_debug_max_deg = 60.0f;
    config.position_period_ms = 5U;
    config.home_speed_params = (PID_Incremental_Param_Config){
        .kp = 0.25f, .ki = 5.0f, .kd = 0.0f, .output_limit = 0.35f,
        .deadband = 0.1f,
    };
    config.position_speed_params = (PID_Incremental_Param_Config){
        .kp = 0.25f, .ki = 5.0f, .kd = 0.0f, .output_limit = 0.6f,
        .deadband = 0.1f,
    };
    config.position_params = (PID_Position_Param_Config){
        .kp = 2.0f, .ki = 0.0f, .kd = 0.0f, .output_limit = 15.0f,
        .deadband = 0.0f,
    };
    return config;
}

static app_balance_step_input_t make_feedback(uint32_t now_ms)
{
    return (app_balance_step_input_t){
        .now_ms = now_ms,
        .feedback_valid = true,
        .feedback_position_deg = 12.5f,
        .feedback_output_speed_rpm = -8.0f,
        .feedback_current_a = -0.1f,
        .requested_target_position_deg = 0.0f,
    };
}

static void test_homing_requires_feedback_and_uses_negative_search_speed(void)
{
    app_balance_t balance;
    app_balance_config_t config = make_config();
    app_balance_step_input_t input = make_feedback(0U);
    app_balance_step_output_t output;

    app_balance_init(&balance, &config);
    input.feedback_valid = false;
    app_balance_step(&balance, &input, &output);
    assert(output.state == APP_BALANCE_STATE_WAIT_FEEDBACK);
    assert(output.commanded_current_a == 0.0f);

    input.feedback_valid = true;
    app_balance_step(&balance, &input, &output);
    assert(output.state == APP_BALANCE_STATE_HOME_SEEK);
    assert(output.commanded_current_a == 0.0f);

    input.now_ms = 1U;
    input.feedback_output_speed_rpm = 0.0f;
    app_balance_step(&balance, &input, &output);
    assert(output.state == APP_BALANCE_STATE_HOME_SEEK);
    assert(output.target_output_speed_rpm == -8.0f);
    assert(output.commanded_current_a <= 0.0f);
    assert(fabsf(output.commanded_current_a) <= 0.35f);
}

static void test_homing_confirms_stall_then_establishes_zero(void)
{
    app_balance_t balance;
    app_balance_config_t config = make_config();
    app_balance_step_input_t input = make_feedback(0U);
    app_balance_step_output_t output;

    app_balance_init(&balance, &config);
    app_balance_step(&balance, &input, &output);
    input.now_ms = 1U;
    input.feedback_output_speed_rpm = 0.2f;
    input.feedback_current_a = -0.3f;
    app_balance_step(&balance, &input, &output);
    assert(output.state == APP_BALANCE_STATE_HOME_CONFIRM);
    assert(!output.zero_valid);

    input.now_ms = 300U;
    app_balance_step(&balance, &input, &output);
    assert(output.state == APP_BALANCE_STATE_HOME_CONFIRM);

    input.now_ms = 301U;
    app_balance_step(&balance, &input, &output);
    assert(output.state == APP_BALANCE_STATE_HOMED_IDLE);
    assert(output.zero_valid);
    assert(fabsf(output.zero_offset_deg - 12.5f) < 0.0001f);
    assert(fabsf(output.feedback_position_deg) < 0.0001f);
    assert(output.commanded_current_a == 0.0f);
}

static void test_position_clamps_target_and_rejects_nonfinite_value(void)
{
    app_balance_t balance;
    app_balance_config_t config = make_config();
    app_balance_step_input_t input = make_feedback(0U);
    app_balance_step_output_t output;

    app_balance_init(&balance, &config);
    app_balance_step(&balance, &input, &output);
    input.now_ms = 1U;
    input.feedback_output_speed_rpm = 0.0f;
    input.feedback_current_a = -0.3f;
    app_balance_step(&balance, &input, &output);
    input.now_ms = 301U;
    app_balance_step(&balance, &input, &output);
    assert(output.state == APP_BALANCE_STATE_HOMED_IDLE);

    input.now_ms = 306U;
    input.feedback_current_a = 0.0f;
    input.requested_target_position_deg = 100.0f;
    app_balance_step(&balance, &input, &output);
    assert(output.state == APP_BALANCE_STATE_POSITION);
    assert(output.target_clamped);
    assert(output.active_target_position_deg == 30.0f);
    assert(output.target_output_speed_rpm == 15.0f);
    assert(output.commanded_current_a > 0.0f);
    assert(output.commanded_current_a <= 0.6f);

    input.now_ms = 307U;
    input.requested_target_position_deg = NAN;
    app_balance_step(&balance, &input, &output);
    assert(output.state == APP_BALANCE_STATE_FAULT);
    assert(output.fault == APP_BALANCE_FAULT_INVALID_TARGET);
    assert(output.commanded_current_a == 0.0f);
}

static void test_position_uses_a_separate_active_minimum_after_homing(void)
{
    app_balance_t balance;
    app_balance_config_t config = make_config();
    app_balance_step_input_t input = make_feedback(0U);
    app_balance_step_output_t output;

    config.position_active_min_deg = 5.0f;
    app_balance_init(&balance, &config);
    app_balance_step(&balance, &input, &output);
    input.now_ms = 1U;
    input.feedback_output_speed_rpm = 0.2f;
    input.feedback_current_a = -0.3f;
    app_balance_step(&balance, &input, &output);
    input.now_ms = 301U;
    app_balance_step(&balance, &input, &output);
    assert(output.state == APP_BALANCE_STATE_HOMED_IDLE);

    input.now_ms = 306U;
    input.feedback_current_a = 0.0f;
    input.requested_target_position_deg = 0.0f;
    app_balance_step(&balance, &input, &output);
    assert(output.state == APP_BALANCE_STATE_HOMED_IDLE);
    assert(!output.target_clamped);
    assert(output.commanded_current_a == 0.0f);

    input.now_ms = 307U;
    input.requested_target_position_deg = 0.1f;
    app_balance_step(&balance, &input, &output);
    assert(output.state == APP_BALANCE_STATE_POSITION);
    assert(output.target_clamped);
    assert(output.active_target_position_deg == 5.0f);
}

static void test_extended_range_switch_uses_debug_bounds_without_removing_soft_limits(void)
{
    app_balance_t balance;
    app_balance_config_t config = make_config();
    app_balance_step_input_t input = make_feedback(0U);
    app_balance_step_output_t output;

    config.position_active_min_deg = 5.0f;
    app_balance_init(&balance, &config);
    app_balance_step(&balance, &input, &output);
    input.now_ms = 1U;
    input.feedback_output_speed_rpm = 0.2f;
    input.feedback_current_a = -0.3f;
    app_balance_step(&balance, &input, &output);
    input.now_ms = 301U;
    app_balance_step(&balance, &input, &output);
    assert(output.state == APP_BALANCE_STATE_HOMED_IDLE);

    input.now_ms = 306U;
    input.feedback_current_a = 0.0f;
    input.requested_target_position_deg = 45.0f;
    app_balance_step(&balance, &input, &output);
    assert(output.target_clamped);
    assert(output.active_target_position_deg == 30.0f);
    assert(!output.extended_position_range_active);

    input.now_ms = 311U;
    input.allow_extended_position_range = true;
    app_balance_step(&balance, &input, &output);
    assert(!output.target_clamped);
    assert(output.active_target_position_deg == 45.0f);
    assert(output.extended_position_range_active);

    input.now_ms = 316U;
    input.requested_target_position_deg = 100.0f;
    app_balance_step(&balance, &input, &output);
    assert(output.target_clamped);
    assert(output.active_target_position_deg == 60.0f);
    assert(output.extended_position_range_active);
}

static void test_timeout_feedback_loss_and_manual_rehome_stay_safe(void)
{
    app_balance_t balance;
    app_balance_config_t config = make_config();
    app_balance_step_input_t input = make_feedback(0U);
    app_balance_step_output_t output;

    config.home_timeout_ms = 10U;
    config.home_confirm_ms = 1U;
    app_balance_init(&balance, &config);
    app_balance_step(&balance, &input, &output);
    input.now_ms = 11U;
    app_balance_step(&balance, &input, &output);
    assert(output.state == APP_BALANCE_STATE_FAULT);
    assert(output.fault == APP_BALANCE_FAULT_HOME_TIMEOUT);
    assert(output.commanded_current_a == 0.0f);

    input.now_ms = 12U;
    input.rehome_request = true;
    app_balance_step(&balance, &input, &output);
    assert(output.rehome_request_consumed);
    assert(output.state == APP_BALANCE_STATE_WAIT_FEEDBACK);
    assert(output.commanded_current_a == 0.0f);

    input.now_ms = 13U;
    input.rehome_request = false;
    input.feedback_valid = true;
    app_balance_step(&balance, &input, &output);
    assert(output.state == APP_BALANCE_STATE_HOME_SEEK);

    input.now_ms = 14U;
    input.feedback_valid = false;
    app_balance_step(&balance, &input, &output);
    assert(output.state == APP_BALANCE_STATE_FAULT);
    assert(output.fault == APP_BALANCE_FAULT_FEEDBACK_LOST);
    assert(output.commanded_current_a == 0.0f);
}

int main(void)
{
    test_homing_requires_feedback_and_uses_negative_search_speed();
    test_homing_confirms_stall_then_establishes_zero();
    test_position_clamps_target_and_rejects_nonfinite_value();
    test_position_uses_a_separate_active_minimum_after_homing();
    test_extended_range_switch_uses_debug_bounds_without_removing_soft_limits();
    test_timeout_feedback_loss_and_manual_rehome_stay_safe();
    return 0;
}
