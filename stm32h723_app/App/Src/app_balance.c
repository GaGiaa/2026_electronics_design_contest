#include "app_balance.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static float app_balance_absf(float value)
{
    return value < 0.0f ? -value : value;
}

static float app_balance_clamp(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static float app_balance_active_minimum(const app_balance_t *balance,
                                        const app_balance_step_input_t *input)
{
    return input->allow_extended_position_range ?
               balance->config.position_debug_active_min_deg :
               balance->config.position_active_min_deg;
}

static float app_balance_active_maximum(const app_balance_t *balance,
                                        const app_balance_step_input_t *input)
{
    return input->allow_extended_position_range ? balance->config.position_debug_max_deg :
                                                  balance->config.position_max_deg;
}

static bool app_balance_config_is_valid(const app_balance_config_t *config)
{
    return config != NULL && isfinite(config->home_search_output_speed_rpm) &&
           config->home_search_output_speed_rpm < 0.0f &&
           isfinite(config->home_current_limit_a) && config->home_current_limit_a > 0.0f &&
           isfinite(config->home_stall_speed_rpm) && config->home_stall_speed_rpm >= 0.0f &&
           isfinite(config->home_stall_current_ratio) &&
           config->home_stall_current_ratio > 0.0f && config->home_stall_current_ratio <= 1.0f &&
           config->home_confirm_ms > 0U && config->home_timeout_ms > config->home_confirm_ms &&
           isfinite(config->position_min_deg) && isfinite(config->position_max_deg) &&
           config->position_min_deg <= 0.0f && config->position_max_deg > config->position_min_deg &&
           isfinite(config->position_active_min_deg) &&
           config->position_active_min_deg >= config->position_min_deg &&
           config->position_active_min_deg <= config->position_max_deg &&
           isfinite(config->position_debug_active_min_deg) &&
           config->position_debug_active_min_deg >= config->position_min_deg &&
           isfinite(config->position_debug_max_deg) &&
           config->position_debug_max_deg > config->position_debug_active_min_deg &&
           config->position_period_ms > 0U;
}

static bool app_balance_feedback_is_valid(const app_balance_step_input_t *input)
{
    return input != NULL && input->feedback_valid && isfinite(input->feedback_position_deg) &&
           isfinite(input->feedback_output_speed_rpm) && isfinite(input->feedback_current_a);
}

static void app_balance_reset_pids(app_balance_t *balance,
                                   const PID_Incremental_Param_Config *speed_params)
{
    PID_Incremental_Param_Config params = *speed_params;

    params.output_limit = balance->config.home_current_limit_a;
    PID_Incremental_Init(&balance->speed_pid, &params, 0.001f);
    PID_Position_Init(&balance->position_pid, &balance->config.position_params,
                      (float)balance->config.position_period_ms / 1000.0f);
    balance->target_output_speed_rpm = 0.0f;
}

static void app_balance_start_seek(app_balance_t *balance, uint32_t now_ms)
{
    app_balance_reset_pids(balance, &balance->config.home_speed_params);
    balance->state = APP_BALANCE_STATE_HOME_SEEK;
    balance->fault = APP_BALANCE_FAULT_NONE;
    balance->zero_offset_deg = 0.0f;
    balance->zero_valid = false;
    balance->home_started_ms = now_ms;
    balance->stall_started_ms = 0U;
}

static void app_balance_start_position(app_balance_t *balance, uint32_t now_ms)
{
    PID_Incremental_Param_Config params = balance->config.position_speed_params;

    params.output_limit = balance->config.position_speed_params.output_limit;
    PID_Incremental_Init(&balance->speed_pid, &params, 0.001f);
    PID_Position_Init(&balance->position_pid, &balance->config.position_params,
                      (float)balance->config.position_period_ms / 1000.0f);
    balance->position_last_update_ms = now_ms - balance->config.position_period_ms;
    balance->target_output_speed_rpm = 0.0f;
    balance->state = APP_BALANCE_STATE_POSITION;
}

static void app_balance_enter_fault(app_balance_t *balance, app_balance_fault_t fault)
{
    PID_Incremental_Reset(&balance->speed_pid);
    PID_Position_Reset(&balance->position_pid);
    balance->state = APP_BALANCE_STATE_FAULT;
    balance->fault = fault;
    balance->target_output_speed_rpm = 0.0f;
    balance->zero_valid = false;
}

static bool app_balance_stall_detected(const app_balance_t *balance,
                                       const app_balance_step_input_t *input)
{
    const float minimum_current = balance->config.home_current_limit_a *
                                  balance->config.home_stall_current_ratio;

    return app_balance_absf(input->feedback_output_speed_rpm) <=
               balance->config.home_stall_speed_rpm &&
           app_balance_absf(input->feedback_current_a) >= minimum_current;
}

static void app_balance_update_target(const app_balance_t *balance,
                                      const app_balance_step_input_t *input,
                                      app_balance_step_output_t *output)
{
    const float active_minimum = app_balance_active_minimum(balance, input);
    const float active_maximum = app_balance_active_maximum(balance, input);

    output->requested_target_position_deg = input->requested_target_position_deg;
    if (!isfinite(input->requested_target_position_deg)) {
        return;
    }
    if (input->requested_target_position_deg <= balance->config.position_min_deg) {
        output->active_target_position_deg = app_balance_clamp(
            input->requested_target_position_deg, balance->config.position_min_deg,
            active_maximum);
    } else {
        output->active_target_position_deg = app_balance_clamp(
            input->requested_target_position_deg, active_minimum, active_maximum);
    }
    output->target_clamped = output->active_target_position_deg != input->requested_target_position_deg;
}

static bool app_balance_position_is_requested(const app_balance_t *balance,
                                              const app_balance_step_input_t *input)
{
    return input->requested_target_position_deg > balance->config.position_min_deg;
}

void app_balance_init(app_balance_t *balance, const app_balance_config_t *config)
{
    if (balance == NULL) {
        return;
    }

    (void)memset(balance, 0, sizeof(*balance));
    if (!app_balance_config_is_valid(config)) {
        balance->state = APP_BALANCE_STATE_FAULT;
        balance->fault = APP_BALANCE_FAULT_INVALID_CONFIG;
        return;
    }

    balance->config = *config;
    app_balance_reset_pids(balance, &config->home_speed_params);
    balance->state = APP_BALANCE_STATE_WAIT_FEEDBACK;
    balance->fault = APP_BALANCE_FAULT_NONE;
}

void app_balance_step(app_balance_t *balance,
                      const app_balance_step_input_t *input,
                      app_balance_step_output_t *output)
{
    bool feedback_valid;
    bool entered_position = false;
    bool rehome_reset = false;
    float relative_position = 0.0f;

    if (output == NULL) {
        return;
    }
    (void)memset(output, 0, sizeof(*output));
    if (balance == NULL || input == NULL) {
        output->state = APP_BALANCE_STATE_FAULT;
        output->fault = APP_BALANCE_FAULT_INVALID_CONFIG;
        return;
    }
    output->extended_position_range_active = input->allow_extended_position_range;

    feedback_valid = app_balance_feedback_is_valid(input);
    if (feedback_valid && balance->zero_valid) {
        relative_position = input->feedback_position_deg - balance->zero_offset_deg;
    }

    if (input->rehome_request && !balance->rehome_request_seen &&
        balance->state == APP_BALANCE_STATE_FAULT) {
        app_balance_reset_pids(balance, &balance->config.home_speed_params);
        balance->state = APP_BALANCE_STATE_WAIT_FEEDBACK;
        balance->fault = APP_BALANCE_FAULT_NONE;
        balance->zero_offset_deg = 0.0f;
        balance->zero_valid = false;
        output->rehome_request_consumed = true;
        rehome_reset = true;
    }
    balance->rehome_request_seen = input->rehome_request;

    if (!rehome_reset) {
    switch (balance->state) {
    case APP_BALANCE_STATE_WAIT_FEEDBACK:
        if (feedback_valid) {
            app_balance_start_seek(balance, input->now_ms);
        }
        break;

    case APP_BALANCE_STATE_HOME_SEEK:
        if (!feedback_valid) {
            app_balance_enter_fault(balance, APP_BALANCE_FAULT_FEEDBACK_LOST);
        } else if ((uint32_t)(input->now_ms - balance->home_started_ms) >=
                   balance->config.home_timeout_ms) {
            app_balance_enter_fault(balance, APP_BALANCE_FAULT_HOME_TIMEOUT);
        } else {
            balance->target_output_speed_rpm = balance->config.home_search_output_speed_rpm;
            output->commanded_current_a = PID_Incremental_Calc(&balance->speed_pid,
                                                                 balance->target_output_speed_rpm,
                                                                 input->feedback_output_speed_rpm);
            output->commanded_current_a = app_balance_clamp(output->commanded_current_a,
                                                              -balance->config.home_current_limit_a,
                                                              balance->config.home_current_limit_a);
            if (app_balance_stall_detected(balance, input)) {
                balance->state = APP_BALANCE_STATE_HOME_CONFIRM;
                balance->stall_started_ms = input->now_ms;
            }
        }
        break;

    case APP_BALANCE_STATE_HOME_CONFIRM:
        if (!feedback_valid) {
            app_balance_enter_fault(balance, APP_BALANCE_FAULT_FEEDBACK_LOST);
        } else if ((uint32_t)(input->now_ms - balance->home_started_ms) >=
                   balance->config.home_timeout_ms) {
            app_balance_enter_fault(balance, APP_BALANCE_FAULT_HOME_TIMEOUT);
        } else if (!app_balance_stall_detected(balance, input)) {
            balance->state = APP_BALANCE_STATE_HOME_SEEK;
            balance->stall_started_ms = 0U;
        } else if ((uint32_t)(input->now_ms - balance->stall_started_ms) >=
                   balance->config.home_confirm_ms) {
            balance->zero_offset_deg = input->feedback_position_deg;
            balance->zero_valid = true;
            balance->state = APP_BALANCE_STATE_HOMED_IDLE;
            PID_Incremental_Reset(&balance->speed_pid);
            PID_Position_Reset(&balance->position_pid);
            balance->target_output_speed_rpm = 0.0f;
            relative_position = 0.0f;
        } else {
            balance->target_output_speed_rpm = balance->config.home_search_output_speed_rpm;
            output->commanded_current_a = PID_Incremental_Calc(&balance->speed_pid,
                                                                 balance->target_output_speed_rpm,
                                                                 input->feedback_output_speed_rpm);
            output->commanded_current_a = app_balance_clamp(output->commanded_current_a,
                                                              -balance->config.home_current_limit_a,
                                                              balance->config.home_current_limit_a);
        }
        break;

    case APP_BALANCE_STATE_HOMED_IDLE:
        if (!feedback_valid) {
            app_balance_enter_fault(balance, APP_BALANCE_FAULT_FEEDBACK_LOST);
        } else if (!isfinite(input->requested_target_position_deg)) {
            app_balance_enter_fault(balance, APP_BALANCE_FAULT_INVALID_TARGET);
        } else if (app_balance_position_is_requested(balance, input)) {
            app_balance_start_position(balance, input->now_ms);
            entered_position = true;
        }
        break;

    case APP_BALANCE_STATE_POSITION:
        if (!feedback_valid) {
            app_balance_enter_fault(balance, APP_BALANCE_FAULT_FEEDBACK_LOST);
        } else if (!isfinite(input->requested_target_position_deg)) {
            app_balance_enter_fault(balance, APP_BALANCE_FAULT_INVALID_TARGET);
        } else if (!app_balance_position_is_requested(balance, input)) {
            PID_Incremental_Reset(&balance->speed_pid);
            PID_Position_Reset(&balance->position_pid);
            balance->target_output_speed_rpm = 0.0f;
            balance->state = APP_BALANCE_STATE_HOMED_IDLE;
        } else {
            if ((uint32_t)(input->now_ms - balance->position_last_update_ms) >=
                balance->config.position_period_ms) {
                const float target = app_balance_clamp(input->requested_target_position_deg,
                                                       app_balance_active_minimum(balance, input),
                                                       app_balance_active_maximum(balance, input));
                balance->target_output_speed_rpm = PID_Position_Calc(&balance->position_pid,
                                                                       target, relative_position);
                balance->position_last_update_ms = input->now_ms;
            }
            output->commanded_current_a = PID_Incremental_Calc(&balance->speed_pid,
                                                                 balance->target_output_speed_rpm,
                                                                 input->feedback_output_speed_rpm);
            output->commanded_current_a = app_balance_clamp(
                output->commanded_current_a,
                -balance->config.position_speed_params.output_limit,
                balance->config.position_speed_params.output_limit);
        }
        break;

    case APP_BALANCE_STATE_FAULT:
    default:
        break;
    }
    }

    if (entered_position) {
        const float target = app_balance_clamp(input->requested_target_position_deg,
                                               app_balance_active_minimum(balance, input),
                                               app_balance_active_maximum(balance, input));
        balance->target_output_speed_rpm = PID_Position_Calc(&balance->position_pid,
                                                               target, relative_position);
        balance->position_last_update_ms = input->now_ms;
        output->commanded_current_a = PID_Incremental_Calc(&balance->speed_pid,
                                                             balance->target_output_speed_rpm,
                                                             input->feedback_output_speed_rpm);
        output->commanded_current_a = app_balance_clamp(
            output->commanded_current_a,
            -balance->config.position_speed_params.output_limit,
            balance->config.position_speed_params.output_limit);
    }

    app_balance_update_target(balance, input, output);
    output->state = balance->state;
    output->fault = balance->fault;
    output->zero_valid = balance->zero_valid;
    output->zero_offset_deg = balance->zero_offset_deg;
    output->feedback_position_deg = relative_position;
    output->target_output_speed_rpm = balance->target_output_speed_rpm;
    if (balance->state == APP_BALANCE_STATE_FAULT ||
        balance->state == APP_BALANCE_STATE_WAIT_FEEDBACK ||
        balance->state == APP_BALANCE_STATE_HOMED_IDLE) {
        output->commanded_current_a = 0.0f;
    }
}
