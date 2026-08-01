#include "app_ball_position_control.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static bool app_ball_position_config_is_valid(
    const app_ball_position_control_config_t *config)
{
    const PID_Position_Param_Config *params;
    uint32_t index;

    if (config == NULL || config->period_ms != APP_BALL_POSITION_CONTROL_PERIOD_MS ||
        config->max_age_ms == 0U || !isfinite(config->safe_motor_position_deg) ||
        !isfinite(config->output_limit_deg) || config->output_limit_deg <= 0.0f ||
        !isfinite(config->sign) || (config->sign != -1.0f && config->sign != 1.0f) ||
        !isfinite(config->deadband_mm) || config->deadband_mm < 0.0f ||
        !isfinite(config->engage_error_mm) || !isfinite(config->release_error_mm) ||
        config->engage_error_mm < config->release_error_mm ||
        config->release_error_mm < 0.0f ||
        !isfinite(config->breakaway_pulse_deg) ||
        config->breakaway_pulse_deg <= 0.0f ||
        config->breakaway_stall_time_ms == 0U ||
        !isfinite(config->breakaway_min_motion_mm) ||
        config->breakaway_min_motion_mm < 0.0f ||
        config->breakaway_duration_ms == 0U) {
        return false;
    }

    if (config->use_hold_position_map) {
        for (index = 0U; index < APP_BALL_POSITION_HOLD_MAP_POINT_COUNT; ++index) {
            if (!isfinite(config->hold_position_mm[index]) ||
                !isfinite(config->hold_motor_position_deg[index]) ||
                (index > 0U &&
                 config->hold_position_mm[index] <= config->hold_position_mm[index - 1U])) {
                return false;
            }
        }
    }

    params = &config->pid_params;
    return isfinite(params->kp) && isfinite(params->ki) && isfinite(params->kd) &&
           isfinite(params->output_limit) && isfinite(params->deadband) &&
           params->kp >= 0.0f && params->ki >= 0.0f && params->kd >= 0.0f &&
           params->output_limit > 0.0f && params->deadband >= 0.0f;
}

static float app_ball_position_hold_motor_position(
    const app_ball_position_control_t *control, float position_mm)
{
    const app_ball_position_control_config_t *config = &control->config;
    uint32_t index;

    if (!config->use_hold_position_map) {
        return config->safe_motor_position_deg;
    }

    if (position_mm <= config->hold_position_mm[0U]) {
        return config->hold_motor_position_deg[0U];
    }
    for (index = 1U; index < APP_BALL_POSITION_HOLD_MAP_POINT_COUNT; ++index) {
        if (position_mm <= config->hold_position_mm[index]) {
            const float lower_position = config->hold_position_mm[index - 1U];
            const float ratio = (position_mm - lower_position) /
                                (config->hold_position_mm[index] - lower_position);
            return config->hold_motor_position_deg[index - 1U] +
                   ratio * (config->hold_motor_position_deg[index] -
                            config->hold_motor_position_deg[index - 1U]);
        }
    }
    return config->hold_motor_position_deg[APP_BALL_POSITION_HOLD_MAP_POINT_COUNT - 1U];
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
    output->pid_offset_deg = control->config.sign * control->pid.output +
                             control->breakaway_offset_deg;
    output->integral = control->pid.integral;
    output->drive_active = control->drive_active;
    output->hold_motor_position_deg = control->hold_motor_position_deg;
    output->target_motor_position_deg = control->target_motor_position_deg;
    output->breakaway_active = control->breakaway_active;
    output->breakaway_trigger_count = control->breakaway_trigger_count;
    output->breakaway_stall_elapsed_ms = control->breakaway_stall_elapsed_ms;
    output->breakaway_offset_deg = control->breakaway_offset_deg;
}

static void app_ball_position_clear_breakaway(app_ball_position_control_t *control)
{
    control->has_last_measured = false;
    control->last_measured_mm = 0.0f;
    control->breakaway_stall_elapsed_ms = 0U;
    control->breakaway_pulse_elapsed_ms = 0U;
    control->breakaway_cooldown_remaining_ms = 0U;
    control->breakaway_active = false;
    control->breakaway_offset_deg = 0.0f;
}

static uint32_t app_ball_position_add_time(uint32_t elapsed_ms,
                                           uint32_t delta_ms)
{
    if (UINT32_MAX - elapsed_ms < delta_ms) {
        return UINT32_MAX;
    }
    return elapsed_ms + delta_ms;
}

static void app_ball_position_update_breakaway(
    app_ball_position_control_t *control,
    const app_ball_position_control_input_t *input,
    uint32_t elapsed_ms,
    float error_mm)
{
    const app_ball_position_control_config_t *config = &control->config;
    float motion_mm = 0.0f;

    if (!config->breakaway_enable || !control->drive_active ||
        fabsf(error_mm) < config->engage_error_mm) {
        app_ball_position_clear_breakaway(control);
        return;
    }

    if (control->has_last_measured) {
        motion_mm = fabsf(input->measured_mm - control->last_measured_mm);
        if (motion_mm <= config->breakaway_min_motion_mm) {
            control->breakaway_stall_elapsed_ms = app_ball_position_add_time(
                control->breakaway_stall_elapsed_ms, elapsed_ms);
        } else {
            control->breakaway_stall_elapsed_ms = 0U;
        }
    }
    control->last_measured_mm = input->measured_mm;
    control->has_last_measured = true;

    if (control->breakaway_cooldown_remaining_ms > elapsed_ms) {
        control->breakaway_cooldown_remaining_ms -= elapsed_ms;
    } else {
        control->breakaway_cooldown_remaining_ms = 0U;
    }

    if (control->breakaway_active) {
        control->breakaway_pulse_elapsed_ms = app_ball_position_add_time(
            control->breakaway_pulse_elapsed_ms, elapsed_ms);
        if (control->breakaway_pulse_elapsed_ms >= config->breakaway_duration_ms) {
            control->breakaway_active = false;
            control->breakaway_pulse_elapsed_ms = 0U;
            control->breakaway_offset_deg = 0.0f;
        }
    }

    if (!control->breakaway_active &&
        control->breakaway_cooldown_remaining_ms == 0U &&
        control->breakaway_stall_elapsed_ms >= config->breakaway_stall_time_ms) {
        const float error_direction = error_mm >= 0.0f ? 1.0f : -1.0f;

        control->breakaway_active = true;
        control->breakaway_pulse_elapsed_ms = 0U;
        control->breakaway_stall_elapsed_ms = 0U;
        control->breakaway_cooldown_remaining_ms = config->breakaway_cooldown_ms;
        control->breakaway_trigger_count++;
        control->breakaway_offset_deg = control->config.sign * error_direction *
                                        config->breakaway_pulse_deg;
    }
}

static void app_ball_position_hold(app_ball_position_control_t *control,
                                   app_ball_position_control_state_t state,
                                   app_ball_position_control_fault_t fault,
                                   app_ball_position_control_output_t *output)
{
    PID_Position_Reset(&control->pid);
    control->has_last_update = false;
    control->drive_active = false;
    app_ball_position_clear_breakaway(control);
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
        .safe_motor_position_deg = 134.0f,
        .pid_params = {
            .kp = 2.0f,
            .ki = 0.0f,
            .kd = 0.8f,
            .output_limit = 360.0f,
            .deadband = 0.5f,
        },
        .output_limit_deg = 360.0f,
        .sign = 1.0f,
        .use_hold_position_map = true,
        .deadband_mm = 0.5f,
        .hold_position_mm = {20.0f, 125.0f, 230.0f},
        .hold_motor_position_deg = {134.0f, 134.0f, 134.0f},
        .engage_error_mm = 0.5f,
        .release_error_mm = 0.5f,
        .breakaway_enable = false,
        .breakaway_pulse_deg = 10.0f,
        .breakaway_stall_time_ms = 200U,
        .breakaway_min_motion_mm = 1.0f,
        .breakaway_duration_ms = 60U,
        .breakaway_cooldown_ms = 500U,
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
    control->hold_motor_position_deg = control->config.safe_motor_position_deg;
    control->target_motor_position_deg = control->config.safe_motor_position_deg;
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
    control->drive_active = false;
    control->hold_motor_position_deg = control->config.safe_motor_position_deg;
    control->target_motor_position_deg = control->config.safe_motor_position_deg;
    app_ball_position_clear_breakaway(control);
    control->breakaway_trigger_count = 0U;
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
            float pid_offset_deg = 0.0f;
            const uint32_t elapsed_ms = control->has_last_update ?
                (uint32_t)(input->now_ms - control->last_update_ms) : 0U;

            if (!control->drive_active &&
                fabsf(input->target_mm - input->measured_mm) >=
                    control->config.engage_error_mm) {
                control->drive_active = true;
            } else if (control->drive_active &&
                       fabsf(input->target_mm - input->measured_mm) <=
                           control->config.release_error_mm) {
                control->drive_active = false;
                PID_Position_Reset(&control->pid);
            }

            control->hold_motor_position_deg =
                app_ball_position_hold_motor_position(control, input->measured_mm);
            if (control->drive_active) {
                const float abs_error_mm = fabsf(input->target_mm - input->measured_mm);
                /* Keep hysteresis active, but prevent integral wind-up inside its band. */
                if (abs_error_mm < control->config.engage_error_mm) {
                    (void)PID_Position_Calc_DerivativeOnMeasurement_NoIntegral(
                        &control->pid, input->target_mm, input->measured_mm);
                } else {
                    (void)PID_Position_Calc_DerivativeOnMeasurement(
                        &control->pid, input->target_mm, input->measured_mm);
                }
                pid_offset_deg = control->config.sign * control->pid.output;
                app_ball_position_update_breakaway(
                    control, input, elapsed_ms,
                    input->target_mm - input->measured_mm);
            } else {
                app_ball_position_clear_breakaway(control);
            }
            pid_offset_deg += control->breakaway_offset_deg;
            control->target_motor_position_deg =
                control->hold_motor_position_deg + pid_offset_deg;
            control->last_update_ms = input->now_ms;
            control->has_last_update = true;
        }
    }

    app_ball_position_publish(control, output);
}
