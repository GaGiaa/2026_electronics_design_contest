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

bool app_single_motor_step(PID_Incremental *pid,
                           const app_single_motor_step_input_t *input,
                           uint32_t feedback_timeout_ms,
                           float max_target_output_speed_rpm,
                           float max_command_current_A,
                           app_single_motor_step_output_t *output)
{
    bool valid;

    if (output == NULL) {
        return false;
    }
    *output = (app_single_motor_step_output_t){0};
    if (input == NULL || pid == NULL) {
        output->reset_pid = true;
        output->safety_reason = APP_SINGLE_MOTOR_SAFETY_PARAMS;
        return false;
    }

    output->selected_id = app_single_motor_sanitize_id(input->selected_id, input->default_id);
    valid = !input->configuration_changed && input->enable == 1U && input->feedback_valid &&
            input->feedback_age_ms < feedback_timeout_ms &&
            isfinite(input->target_output_speed_rpm) && isfinite(max_target_output_speed_rpm) &&
            max_target_output_speed_rpm > 0.0f &&
            fabsf(input->target_output_speed_rpm) <= max_target_output_speed_rpm &&
            app_single_motor_params_are_valid(&input->params);
    if (!valid) {
        PID_Incremental_Reset(pid);
        output->reset_pid = true;
        if (input->configuration_changed) {
            output->safety_reason = APP_SINGLE_MOTOR_SAFETY_CONFIGURATION_CHANGED;
        } else if (input->enable != 1U) {
            output->safety_reason = APP_SINGLE_MOTOR_SAFETY_DISABLED;
        } else if (!input->feedback_valid || input->feedback_age_ms >= feedback_timeout_ms) {
            output->safety_reason = APP_SINGLE_MOTOR_SAFETY_FEEDBACK;
        } else if (!isfinite(input->target_output_speed_rpm) || !isfinite(max_target_output_speed_rpm) ||
                   max_target_output_speed_rpm <= 0.0f ||
                   fabsf(input->target_output_speed_rpm) > max_target_output_speed_rpm) {
            output->safety_reason = APP_SINGLE_MOTOR_SAFETY_TARGET;
        } else {
            output->safety_reason = APP_SINGLE_MOTOR_SAFETY_PARAMS;
        }
        return false;
    }

    pid->params = input->params;
    (void)PID_Incremental_Calc(pid, input->target_output_speed_rpm,
                                input->feedback_output_speed_rpm);
    output->active = true;
    output->commanded_current_A = app_single_motor_clamp_current(pid->output, max_command_current_A);
    output->raw_output_A = pid->raw_output;
    output->pid_output_A = pid->output;
    output->pid_p_out_A = pid->p_out;
    output->pid_i_out_A = pid->i_out;
    output->pid_d_out_A = pid->d_out;
    output->safety_reason = APP_SINGLE_MOTOR_SAFETY_OK;
    return true;
}
