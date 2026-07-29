#include "app_single_motor.h"

#include <math.h>
#include <stddef.h>

static bool app_single_motor_params_are_valid(const PID_Incremental_Param_Config *params)
{
    return params != NULL && isfinite(params->kp) && isfinite(params->ki) &&
           isfinite(params->kd) && isfinite(params->output_limit) &&
           isfinite(params->deadband) && isfinite(params->integral_output_limit) &&
           isfinite(params->integral_separation_threshold) &&
           isfinite(params->derivative_filter_N) && isfinite(params->output_delta_limit) &&
           params->kp >= 0.0f && params->ki >= 0.0f && params->kd >= 0.0f &&
           params->output_limit > 0.0f && params->deadband >= 0.0f &&
           params->integral_output_limit >= 0.0f &&
           params->integral_separation_threshold >= 0.0f &&
           params->derivative_filter_N >= 0.0f && params->output_delta_limit >= 0.0f;
}

static bool app_single_motor_position_params_are_valid(const PID_Position_Param_Config *params)
{
    return params != NULL && isfinite(params->kp) && isfinite(params->ki) &&
           isfinite(params->kd) && isfinite(params->output_limit) &&
           isfinite(params->deadband) && params->kp >= 0.0f && params->ki >= 0.0f &&
           params->kd >= 0.0f && params->output_limit > 0.0f && params->deadband >= 0.0f;
}

static bool app_single_motor_position_state_is_finite(const PID_Position *pid)
{
    return pid != NULL && isfinite(pid->dt_s) && isfinite(pid->integral) &&
           isfinite(pid->last_error) && isfinite(pid->p_out) && isfinite(pid->i_out) &&
           isfinite(pid->d_out) && isfinite(pid->output);
}

static float app_single_motor_clamp_current(float value, float limit)
{
    if (!isfinite(value)) {
        return 0.0f;
    }
    if (!isfinite(limit) || limit <= 0.0f) {
        return 0.0f;
    }
    if (value > limit) {
        value = limit;
    } else if (value < -limit) {
        value = -limit;
    }
    return value;
}

static float app_single_motor_clamp_signed(float value, float limit)
{
    if (!isfinite(value) || !isfinite(limit) || limit <= 0.0f) {
        return 0.0f;
    }
    if (value > limit) {
        return limit;
    }
    if (value < -limit) {
        return -limit;
    }
    return value;
}

uint32_t app_single_motor_sanitize_id(uint32_t requested_id, uint32_t default_id)
{
    if (requested_id >= 1U && requested_id <= 3U) {
        return requested_id;
    }
    if (default_id >= 1U && default_id <= 3U) {
        return default_id;
    }
    return 1U;
}

float app_single_motor_sanitize_max_output_speed_rpm(float requested_limit)
{
    if (!isfinite(requested_limit) || requested_limit <= 0.0f) {
        return 0.0f;
    }
    return requested_limit;
}

bool app_single_motor_step(PID_Incremental *speed_pid,
                           PID_Position *position_pid,
                           const app_single_motor_step_input_t *input,
                           uint32_t feedback_timeout_ms,
                           float max_target_output_speed_rpm,
                           float max_command_current_A,
                           app_single_motor_step_output_t *output)
{
    bool valid;
    float target_output_speed_rpm;

    if (output == NULL) {
        return false;
    }
    *output = (app_single_motor_step_output_t){0};
    if (input == NULL || speed_pid == NULL || position_pid == NULL) {
        output->reset_pid = true;
        output->safety_reason = APP_SINGLE_MOTOR_SAFETY_PARAMS;
        return false;
    }

    output->selected_id = app_single_motor_sanitize_id(input->selected_id, input->default_id);
    valid = !input->configuration_changed && input->enable == 1U && input->feedback_valid &&
            input->feedback_age_ms < feedback_timeout_ms &&
            isfinite(max_target_output_speed_rpm) && max_target_output_speed_rpm > 0.0f &&
            app_single_motor_params_are_valid(&input->params);
    if (input->control_mode == APP_SINGLE_MOTOR_CONTROL_MODE_SPEED) {
        valid = valid && isfinite(input->target_output_speed_rpm) &&
                fabsf(input->target_output_speed_rpm) <= max_target_output_speed_rpm;
    } else if (input->control_mode == APP_SINGLE_MOTOR_CONTROL_MODE_POSITION) {
        valid = valid && input->position_reference_valid && isfinite(input->target_position_deg) &&
                isfinite(input->feedback_position_deg) &&
                app_single_motor_position_params_are_valid(&input->position_params) &&
                app_single_motor_position_state_is_finite(position_pid);
    } else {
        valid = false;
    }
    if (!valid) {
        PID_Incremental_Reset(speed_pid);
        PID_Position_Reset(position_pid);
        output->reset_pid = true;
        if (input->configuration_changed) {
            output->safety_reason = APP_SINGLE_MOTOR_SAFETY_CONFIGURATION_CHANGED;
        } else if (input->enable != 1U) {
            output->safety_reason = APP_SINGLE_MOTOR_SAFETY_DISABLED;
        } else if (!input->feedback_valid || input->feedback_age_ms >= feedback_timeout_ms) {
            output->safety_reason = APP_SINGLE_MOTOR_SAFETY_FEEDBACK;
        } else if (input->control_mode != APP_SINGLE_MOTOR_CONTROL_MODE_SPEED &&
                   input->control_mode != APP_SINGLE_MOTOR_CONTROL_MODE_POSITION) {
            output->safety_reason = APP_SINGLE_MOTOR_SAFETY_CONTROL_MODE;
        } else if (input->control_mode == APP_SINGLE_MOTOR_CONTROL_MODE_POSITION &&
                   !input->position_reference_valid) {
            output->safety_reason = APP_SINGLE_MOTOR_SAFETY_POSITION_REFERENCE;
        } else if (input->control_mode == APP_SINGLE_MOTOR_CONTROL_MODE_POSITION &&
                   (!isfinite(input->target_position_deg) || !isfinite(input->feedback_position_deg))) {
            output->safety_reason = APP_SINGLE_MOTOR_SAFETY_POSITION_TARGET;
        } else if (!isfinite(max_target_output_speed_rpm) ||
                   max_target_output_speed_rpm <= 0.0f ||
                   (input->control_mode == APP_SINGLE_MOTOR_CONTROL_MODE_SPEED &&
                    (!isfinite(input->target_output_speed_rpm) ||
                     fabsf(input->target_output_speed_rpm) > max_target_output_speed_rpm))) {
            output->safety_reason = APP_SINGLE_MOTOR_SAFETY_TARGET;
        } else {
            output->safety_reason = APP_SINGLE_MOTOR_SAFETY_PARAMS;
        }
        return false;
    }

    target_output_speed_rpm = input->target_output_speed_rpm;
    if (input->control_mode == APP_SINGLE_MOTOR_CONTROL_MODE_POSITION) {
        position_pid->params = input->position_params;
        if (input->position_update_due) {
            (void)PID_Position_Calc(position_pid, input->target_position_deg,
                                    input->feedback_position_deg);
            if (!app_single_motor_position_state_is_finite(position_pid)) {
                PID_Incremental_Reset(speed_pid);
                PID_Position_Reset(position_pid);
                output->reset_pid = true;
                output->safety_reason = APP_SINGLE_MOTOR_SAFETY_PARAMS;
                return false;
            }
        }
        target_output_speed_rpm = app_single_motor_clamp_signed(position_pid->output,
                                                                  max_target_output_speed_rpm);
    }
    speed_pid->params = input->params;
    (void)PID_Incremental_Calc(speed_pid, target_output_speed_rpm,
                                input->feedback_output_speed_rpm);
    output->active = true;
    output->target_output_speed_rpm = target_output_speed_rpm;
    output->commanded_current_A = app_single_motor_clamp_current(speed_pid->output, max_command_current_A);
    output->raw_output_A = speed_pid->raw_output;
    output->pid_output_A = speed_pid->output;
    output->pid_p_out_A = speed_pid->p_out;
    output->pid_i_out_A = speed_pid->i_out;
    output->pid_d_out_A = speed_pid->d_out;
    output->position_output_rpm = input->control_mode == APP_SINGLE_MOTOR_CONTROL_MODE_POSITION ?
                                  target_output_speed_rpm : 0.0f;
    output->position_p_out_rpm = input->control_mode == APP_SINGLE_MOTOR_CONTROL_MODE_POSITION ?
                                 position_pid->p_out : 0.0f;
    output->position_i_out_rpm = input->control_mode == APP_SINGLE_MOTOR_CONTROL_MODE_POSITION ?
                                 position_pid->i_out : 0.0f;
    output->position_d_out_rpm = input->control_mode == APP_SINGLE_MOTOR_CONTROL_MODE_POSITION ?
                                 position_pid->d_out : 0.0f;
    output->safety_reason = APP_SINGLE_MOTOR_SAFETY_OK;
    return true;
}
