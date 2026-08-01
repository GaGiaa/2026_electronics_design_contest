#include "app_ball_position_control.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static bool app_ball_position_config_is_valid(
    const app_ball_position_control_config_t *config)
{
    const PID_Position_Param_Config *params;

    if (config == NULL ||
        config->period_ms != APP_BALL_POSITION_CONTROL_PERIOD_MS ||
        config->max_age_ms == 0U || !isfinite(config->output_limit_deg) ||
        config->output_limit_deg <= 0.0f || !isfinite(config->sign) ||
        (config->sign != -1.0f && config->sign != 1.0f) ||
        !isfinite(config->deadband_mm) || config->deadband_mm < 0.0f) {
        return false;
    }

    params = &config->pid_params;
    return isfinite(params->kp) && isfinite(params->ki) && isfinite(params->kd) &&
           isfinite(params->output_limit) && isfinite(params->deadband) &&
           params->kp >= 0.0f && params->ki >= 0.0f && params->kd >= 0.0f &&
           params->output_limit > 0.0f && params->deadband >= 0.0f;
}

static float app_ball_position_clamp(float value, float limit)
{
    if (value > limit) {
        return limit;
    }
    if (value < -limit) {
        return -limit;
    }
    return value;
}

static void app_ball_position_publish(const app_ball_position_control_t *control,
                                      app_ball_position_control_output_t *output)
{
    output->state = control->state;
    output->fault = control->fault;
    output->error_mm = control->pid.last_error;
    output->p_out_deg = control->pid.p_out;
    output->i_out_deg = control->pid.i_out;
    output->d_out_deg = control->pid.d_out;
    output->output_deg = control->pid.output;
    output->target_tilt_deg = app_ball_position_clamp(
        control->config.sign * control->pid.output, control->config.output_limit_deg);
    output->integral = control->pid.integral;
}

static void app_ball_position_hold(app_ball_position_control_t *control,
                                   app_ball_position_control_state_t state,
                                   app_ball_position_control_fault_t fault,
                                   app_ball_position_control_output_t *output)
{
    PID_Position_Reset(&control->pid);
    control->has_last_update = false;
    control->state = state;
    control->fault = fault;
    output->reset = true;
}

void app_ball_position_control_config_default(app_ball_position_control_config_t *config)
{
    if (config == NULL) {
        return;
    }

    *config = (app_ball_position_control_config_t){
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
    };
}

void app_ball_position_control_init(app_ball_position_control_t *control,
                                    const app_ball_position_control_config_t *config)
{
    PID_Position_Param_Config params;

    if (control == NULL) {
        return;
    }

    (void)memset(control, 0, sizeof(*control));
    if (!app_ball_position_config_is_valid(config)) {
        control->state = APP_BALL_POSITION_STATE_FAULT;
        control->fault = APP_BALL_POSITION_FAULT_INVALID_CONFIG;
        return;
    }

    control->config = *config;
    params = control->config.pid_params;
    params.output_limit = control->config.output_limit_deg;
    params.deadband = control->config.deadband_mm;
    PID_Position_Init(&control->pid, &params,
                      (float)control->config.period_ms / 1000.0f);
    control->state = APP_BALL_POSITION_STATE_DISABLED;
    control->fault = APP_BALL_POSITION_FAULT_NONE;
}

void app_ball_position_control_reset(app_ball_position_control_t *control)
{
    if (control == NULL) {
        return;
    }

    PID_Position_Reset(&control->pid);
    control->has_last_update = false;
}

void app_ball_position_control_step(app_ball_position_control_t *control,
                                    const app_ball_position_control_input_t *input,
                                    app_ball_position_control_output_t *output)
{
    bool update_due;

    if (output == NULL) {
        return;
    }
    (void)memset(output, 0, sizeof(*output));
    if (control == NULL || input == NULL) {
        output->state = APP_BALL_POSITION_STATE_FAULT;
        output->fault = APP_BALL_POSITION_FAULT_INVALID_INPUT;
        return;
    }
    if (control->fault == APP_BALL_POSITION_FAULT_INVALID_CONFIG) {
        app_ball_position_publish(control, output);
        return;
    }

    if (!input->enabled) {
        app_ball_position_hold(control, APP_BALL_POSITION_STATE_DISABLED,
                               APP_BALL_POSITION_FAULT_NONE, output);
    } else if (!input->calibration_ready) {
        app_ball_position_hold(control, APP_BALL_POSITION_STATE_HOLD,
                               APP_BALL_POSITION_FAULT_NOT_CALIBRATED, output);
    } else if (!input->id3_ready) {
        app_ball_position_hold(control, APP_BALL_POSITION_STATE_HOLD,
                               APP_BALL_POSITION_FAULT_ID3_NOT_READY, output);
    } else if (!input->vision_valid) {
        app_ball_position_hold(control, APP_BALL_POSITION_STATE_HOLD,
                               APP_BALL_POSITION_FAULT_VISION_INVALID, output);
    } else if (input->vision_age_ms > control->config.max_age_ms) {
        app_ball_position_hold(control, APP_BALL_POSITION_STATE_HOLD,
                               APP_BALL_POSITION_FAULT_VISION_STALE, output);
    } else if (!isfinite(input->target_mm) || !isfinite(input->measured_mm)) {
        app_ball_position_hold(control, APP_BALL_POSITION_STATE_HOLD,
                               APP_BALL_POSITION_FAULT_INVALID_INPUT, output);
    } else {
        update_due = !control->has_last_update ||
                     (uint32_t)(input->now_ms - control->last_update_ms) >=
                         control->config.period_ms;
        control->state = APP_BALL_POSITION_STATE_ACTIVE;
        control->fault = APP_BALL_POSITION_FAULT_NONE;
        output->valid = true;
        output->update_due = update_due;
        if (update_due) {
            (void)PID_Position_Calc(&control->pid, input->target_mm, input->measured_mm);
            control->last_update_ms = input->now_ms;
            control->has_last_update = true;
        }
    }

    app_ball_position_publish(control, output);
}
