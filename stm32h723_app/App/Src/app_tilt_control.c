#include "app_tilt_control.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static float app_tilt_clamp(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static bool app_tilt_config_is_valid(const app_tilt_control_config_t *config)
{
    return config != NULL && isfinite(config->position_min_deg) &&
           isfinite(config->position_max_deg) &&
           config->position_max_deg > config->position_min_deg &&
           config->sample_period_ms > 0U && config->sample_max_age_ms > 0U &&
           (config->pitch_to_tilt_sign == -1.0f || config->pitch_to_tilt_sign == 1.0f) &&
           isfinite(config->pid_params.kp) && isfinite(config->pid_params.ki) &&
           isfinite(config->pid_params.kd) && isfinite(config->pid_params.output_limit) &&
           config->pid_params.output_limit > 0.0f &&
           isfinite(config->pid_params.deadband) && config->pid_params.deadband >= 0.0f &&
           isfinite(config->derivative_filter_N) && config->derivative_filter_N >= 0.0f;
}

static void app_tilt_pid_reset(app_tilt_advanced_pid_t *pid)
{
    if (pid != NULL) {
        (void)memset(pid, 0, sizeof(*pid));
    }
}

static float app_tilt_pid_filter_rate(const app_tilt_control_t *control,
                                      float previous, float measured_rate)
{
    const float filter_n = control->config.derivative_filter_N;

    if (filter_n <= 0.0f) {
        return measured_rate;
    }

    {
        const float dt_s = (float)control->config.sample_period_ms / 1000.0f;
        const float alpha = (filter_n * dt_s) / (1.0f + filter_n * dt_s);
        return previous + alpha * (measured_rate - previous);
    }
}

static float app_tilt_pid_update(app_tilt_control_t *control, float error_deg,
                                 float tilt_deg, bool *rate_clamped)
{
    app_tilt_advanced_pid_t *pid = &control->pid;
    const PID_Position_Param_Config *params = &control->config.pid_params;
    const float dt_s = (float)control->config.sample_period_ms / 1000.0f;
    const float previous_integral = pid->integral;
    float candidate_integral;
    float raw_output;

    if (fabsf(error_deg) <= params->deadband) {
        error_deg = 0.0f;
    }

    pid->measured_tilt_rate_deg_s = 0.0f;
    if (pid->has_measurement_history) {
        pid->measured_tilt_rate_deg_s = (tilt_deg - pid->last_tilt_deg) / dt_s;
    }
    pid->filtered_tilt_rate_deg_s = app_tilt_pid_filter_rate(
        control, pid->filtered_tilt_rate_deg_s, pid->measured_tilt_rate_deg_s);
    pid->p_out = params->kp * error_deg;
    pid->d_out = -params->kd * pid->filtered_tilt_rate_deg_s;
    candidate_integral = previous_integral;
    pid->integral_advanced = false;
    if (params->ki != 0.0f) {
        candidate_integral += error_deg * dt_s;
        pid->integral_advanced = true;
    }
    pid->integral = candidate_integral;
    pid->i_out = params->ki * pid->integral;
    raw_output = pid->p_out + pid->i_out + pid->d_out;
    pid->output = app_tilt_clamp(raw_output, -params->output_limit, params->output_limit);

    /* Do not accumulate I when it would drive an already saturated rate farther out. */
    if (pid->output != raw_output &&
        ((raw_output > params->output_limit && error_deg > 0.0f) ||
         (raw_output < -params->output_limit && error_deg < 0.0f))) {
        pid->integral = previous_integral;
        pid->i_out = params->ki * pid->integral;
        pid->integral_advanced = false;
        raw_output = pid->p_out + pid->i_out + pid->d_out;
        pid->output = app_tilt_clamp(raw_output, -params->output_limit, params->output_limit);
    }

    pid->last_error = error_deg;
    pid->last_tilt_deg = tilt_deg;
    pid->has_measurement_history = true;
    if (rate_clamped != NULL) {
        *rate_clamped = pid->output != raw_output;
    }
    return pid->output;
}

static float app_tilt_pid_reject_integral(app_tilt_control_t *control)
{
    app_tilt_advanced_pid_t *pid = &control->pid;
    const PID_Position_Param_Config *params = &control->config.pid_params;

    if (pid->integral_advanced) {
        pid->integral -= pid->last_error * ((float)control->config.sample_period_ms / 1000.0f);
        pid->integral_advanced = false;
    }
    pid->i_out = params->ki * pid->integral;
    pid->output = app_tilt_clamp(pid->p_out + pid->i_out + pid->d_out,
                                 -params->output_limit, params->output_limit);
    return pid->output;
}

static bool app_tilt_motor_feedback_is_valid(const app_tilt_control_input_t *input)
{
    return input->motor_feedback_valid && isfinite(input->motor_feedback_position_deg);
}

static bool app_tilt_imu_is_valid(const app_tilt_control_t *control,
                                  const app_tilt_control_input_t *input)
{
    return input->imu_online && input->imu_valid && isfinite(input->imu_pitch_deg) &&
           input->imu_sample_age_ms <= control->config.sample_max_age_ms;
}

static void app_tilt_seed_motor_target(app_tilt_control_t *control,
                                       const app_tilt_control_input_t *input,
                                       app_tilt_control_output_t *output)
{
    const float target = app_tilt_clamp(input->motor_feedback_position_deg,
                                        control->config.position_min_deg,
                                        control->config.position_max_deg);

    control->motor_target_position_deg = target;
    control->target_valid = true;
    output->motor_target_clamped = target != input->motor_feedback_position_deg;
}

static app_tilt_fault_t app_tilt_ready_fault(const app_tilt_control_t *control,
                                              const app_tilt_control_input_t *input)
{
    if (!input->balance_zero_valid) {
        return APP_TILT_FAULT_BALANCE_NOT_HOMED;
    }
    if (!app_tilt_motor_feedback_is_valid(input)) {
        return APP_TILT_FAULT_MOTOR_FEEDBACK;
    }
    if (!app_tilt_imu_is_valid(control, input)) {
        return APP_TILT_FAULT_IMU_STALE;
    }
    if (!control->zero_captured_valid) {
        return APP_TILT_FAULT_ZERO_NOT_CAPTURED;
    }
    if (!isfinite(input->target_tilt_deg)) {
        return APP_TILT_FAULT_INVALID_INPUT;
    }
    return APP_TILT_FAULT_NONE;
}

void app_tilt_control_init(app_tilt_control_t *control,
                           const app_tilt_control_config_t *config)
{
    if (control == NULL) {
        return;
    }

    (void)memset(control, 0, sizeof(*control));
    if (!app_tilt_config_is_valid(config)) {
        control->state = APP_TILT_STATE_FAULT;
        control->fault = APP_TILT_FAULT_INVALID_CONFIG;
        return;
    }

    control->config = *config;
    app_tilt_pid_reset(&control->pid);
    control->state = APP_TILT_STATE_DISABLED;
    control->fault = APP_TILT_FAULT_NONE;
}

void app_tilt_control_step(app_tilt_control_t *control,
                           const app_tilt_control_input_t *input,
                           app_tilt_control_output_t *output)
{
    app_tilt_fault_t ready_fault;

    if (output == NULL) {
        return;
    }
    (void)memset(output, 0, sizeof(*output));
    if (control == NULL || input == NULL) {
        output->state = APP_TILT_STATE_FAULT;
        output->fault = APP_TILT_FAULT_INVALID_INPUT;
        return;
    }
    if (control->fault == APP_TILT_FAULT_INVALID_CONFIG) {
        output->state = control->state;
        output->fault = control->fault;
        return;
    }

    if (input->capture_zero_request && !control->capture_zero_request_seen &&
        app_tilt_imu_is_valid(control, input)) {
        control->captured_zero_deg = input->imu_pitch_deg;
        control->zero_captured_valid = true;
        app_tilt_pid_reset(&control->pid);
        control->last_imu_sample_valid = false;
        output->capture_zero_consumed = true;
    }
    control->capture_zero_request_seen = input->capture_zero_request;

    if (app_tilt_motor_feedback_is_valid(input) &&
        (!control->target_valid || !input->enabled || !control->enable_seen)) {
        app_tilt_seed_motor_target(control, input, output);
    }

    if (!input->enabled) {
        app_tilt_pid_reset(&control->pid);
        control->last_imu_sample_valid = false;
        control->state = APP_TILT_STATE_DISABLED;
        control->fault = APP_TILT_FAULT_NONE;
    } else {
        ready_fault = app_tilt_ready_fault(control, input);
        if (ready_fault != APP_TILT_FAULT_NONE) {
            app_tilt_pid_reset(&control->pid);
            control->last_imu_sample_valid = false;
            control->state = APP_TILT_STATE_HOLD;
            control->fault = ready_fault;
        } else {
            const bool new_sample = !control->last_imu_sample_valid ||
                                    input->imu_sample_count != control->last_imu_sample_count;

            control->state = APP_TILT_STATE_ACTIVE;
            control->fault = APP_TILT_FAULT_NONE;
            output->new_imu_sample = new_sample;
            if (new_sample) {
                const float tilt = control->config.pitch_to_tilt_sign *
                                   (input->imu_pitch_deg - control->captured_zero_deg);
                const float rate = app_tilt_pid_update(control, input->target_tilt_deg - tilt,
                                                       tilt, NULL);
                const float requested_position = control->motor_target_position_deg -
                                                 rate * ((float)control->config.sample_period_ms / 1000.0f);
                const float clamped_position = app_tilt_clamp(requested_position,
                                                               control->config.position_min_deg,
                                                               control->config.position_max_deg);

                if (clamped_position != requested_position) {
                    (void)app_tilt_pid_reject_integral(control);
                    control->motor_target_position_deg = app_tilt_clamp(
                        control->motor_target_position_deg -
                            control->pid.output *
                                ((float)control->config.sample_period_ms / 1000.0f),
                        control->config.position_min_deg, control->config.position_max_deg);
                } else {
                    control->motor_target_position_deg = clamped_position;
                }
                control->last_imu_sample_count = input->imu_sample_count;
                control->last_imu_sample_valid = true;
                output->motor_target_clamped = clamped_position != requested_position;
            }
        }
    }

    output->state = control->state;
    output->fault = control->fault;
    output->captured_zero_deg = control->captured_zero_deg;
    output->zero_captured_valid = control->zero_captured_valid;
    output->raw_pitch_deg = input->imu_pitch_deg;
    output->tilt_deg = control->config.pitch_to_tilt_sign *
                       (input->imu_pitch_deg - control->captured_zero_deg);
    output->target_tilt_deg = input->target_tilt_deg;
    output->error_deg = control->pid.last_error;
    output->pid_rate_deg_s = control->pid.output;
    output->pid_p_out_deg_s = control->pid.p_out;
    output->pid_i_out_deg_s = control->pid.i_out;
    output->pid_d_out_deg_s = control->pid.d_out;
    output->pid_integral = control->pid.integral;
    output->measured_tilt_rate_deg_s = control->pid.measured_tilt_rate_deg_s;
    output->filtered_tilt_rate_deg_s = control->pid.filtered_tilt_rate_deg_s;
    output->motor_target_position_deg = control->motor_target_position_deg;
    control->enable_seen = input->enabled;
}
