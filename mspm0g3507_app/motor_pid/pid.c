#include "pid.h"
#include "app_math.h"

#include <math.h>
#include <stddef.h>

static float pid_absf(float value)
{
    return value < 0.0f ? -value : value;
}

static bool pid_has_positive_limit(float limit)
{
    return isfinite(limit) && limit > 0.0f;
}

#if (PID_POSITION_CONFIG_VARIANT == PID_POSITION_VARIANT_ADVANCED)
static float pid_param_or_default(float value, float default_value)
{
    return isfinite(value) && value > 0.0f ? value : default_value;
}

static float pid_first_order_filter(float previous, float input, float n, float dt_s)
{
    const float filter_n = pid_param_or_default(n, 10.0f);
    const float alpha = (filter_n * dt_s) / (1.0f + filter_n * dt_s);

    return previous + alpha * (input - previous);
}
#endif

void PID_Incremental_Init(PID_Incremental *pid, const PID_Incremental_Param_Config *params, float dt_s)
{
    if (pid == NULL || params == NULL) {
        return;
    }

    pid->params = *params;
    pid->dt_s = dt_s > 0.0f ? dt_s : 0.001f;
    PID_Incremental_Reset(pid);
}

void PID_Incremental_Reset(PID_Incremental *pid)
{
    if (pid == NULL) {
        return;
    }

    pid->error = 0.0f;
    pid->last_error = 0.0f;
    pid->prev_error = 0.0f;
    pid->p_out = 0.0f;
    pid->i_out = 0.0f;
    pid->d_out = 0.0f;
    pid->output = 0.0f;
}

float PID_Incremental_Calc(PID_Incremental *pid, float target, float feedback)
{
    float error;

    if (pid == NULL) {
        return 0.0f;
    }

    error = target - feedback;
    if (pid_absf(error) <= pid->params.deadband) {
        error = 0.0f;
    }

    pid->error = error;
    pid->p_out = pid->params.kp * (error - pid->last_error);
    pid->i_out = pid->params.ki * error * pid->dt_s;
    pid->d_out = pid->params.kd * (error - 2.0f * pid->last_error + pid->prev_error) / pid->dt_s;
    pid->output += pid->p_out + pid->i_out + pid->d_out;
    if (pid_has_positive_limit(pid->params.output_limit)) {
        pid->output = App_Math_ClampFloat(pid->output, -pid->params.output_limit, pid->params.output_limit);
    }

    pid->prev_error = pid->last_error;
    pid->last_error = error;
    return pid->output;
}

void PID_Position_Init(PID_Position *pid, const PID_Position_Param_Config *params, float dt_s)
{
    if (pid == NULL || params == NULL) {
        return;
    }

    pid->params = *params;
    pid->dt_s = dt_s > 0.0f ? dt_s : 0.01f;
    PID_Position_Reset(pid);
}

void PID_Position_Reset(PID_Position *pid)
{
    if (pid == NULL) {
        return;
    }

    pid->integral = 0.0f;
    pid->last_error = 0.0f;
    pid->p_out = 0.0f;
    pid->i_out = 0.0f;
    pid->d_out = 0.0f;
    pid->output = 0.0f;
    pid->has_last_error = false;
#if (PID_POSITION_CONFIG_VARIANT == PID_POSITION_VARIANT_ADVANCED)
    pid->last_target = 0.0f;
    pid->last_feedback = 0.0f;
    pid->filtered_derivative = 0.0f;
    pid->filtered_output = 0.0f;
    pid->has_last_sample = false;
#endif
}

float PID_Position_Calc(PID_Position *pid, float target, float feedback)
{
    float error;

    if (pid == NULL) {
        return 0.0f;
    }

    error = target - feedback;
    if (pid_absf(error) <= pid->params.deadband) {
        error = 0.0f;
    }

#if (PID_POSITION_CONFIG_VARIANT == PID_POSITION_VARIANT_ADVANCED)
    {
        const float b = isfinite(pid->params.setpoint_weight_b) ? pid->params.setpoint_weight_b : 1.0f;
        const float c = isfinite(pid->params.setpoint_weight_c) ? pid->params.setpoint_weight_c : 0.0f;
        const float anti_windup_gain = pid_param_or_default(pid->params.anti_windup_gain, 1.0f);
        const float proportional_error = (b * target) - feedback;
        const float derivative_input = (c * target) - feedback;
        float raw_derivative = 0.0f;
        float raw_output;

        if (pid->has_last_sample) {
            raw_derivative = (derivative_input - ((c * pid->last_target) - pid->last_feedback)) / pid->dt_s;
        }
        pid->filtered_derivative = pid_first_order_filter(pid->filtered_derivative,
                                                          raw_derivative,
                                                          pid->params.derivative_filter_N,
                                                          pid->dt_s);
        if (!pid_has_positive_limit(pid->params.integral_separation_threshold) ||
            pid_absf(error) <= pid->params.integral_separation_threshold) {
            pid->integral += error * pid->dt_s;
        }
        if (pid_has_positive_limit(pid->params.I_Outlimit)) {
            pid->integral = App_Math_ClampFloat(pid->integral, -pid->params.I_Outlimit,
                                                pid->params.I_Outlimit);
        }
        pid->p_out = pid->params.kp * proportional_error;
        pid->i_out = pid->params.ki * pid->integral;
        pid->d_out = pid->params.kd * pid->filtered_derivative;
        raw_output = pid->p_out + pid->i_out + pid->d_out;
        pid->output = raw_output;
        if (pid_has_positive_limit(pid->params.output_limit)) {
            pid->output = App_Math_ClampFloat(raw_output, -pid->params.output_limit,
                                              pid->params.output_limit);
            if (pid->params.ki != 0.0f) {
                pid->integral += anti_windup_gain * (pid->output - raw_output) / pid->params.ki * pid->dt_s;
                if (pid_has_positive_limit(pid->params.I_Outlimit)) {
                    pid->integral = App_Math_ClampFloat(pid->integral, -pid->params.I_Outlimit,
                                                        pid->params.I_Outlimit);
                }
                pid->i_out = pid->params.ki * pid->integral;
            }
        }
        if (pid_has_positive_limit(pid->params.output_filter_N)) {
            pid->filtered_output = pid_first_order_filter(pid->filtered_output, pid->output,
                                                          pid->params.output_filter_N, pid->dt_s);
            pid->output = pid->filtered_output;
        } else {
            pid->filtered_output = pid->output;
        }
        pid->last_target = target;
        pid->last_feedback = feedback;
        pid->has_last_sample = true;
    }
#else
    {
        float derivative;

    pid->integral += error * pid->dt_s;
    derivative = pid->has_last_error ? ((error - pid->last_error) / pid->dt_s) : 0.0f;
    pid->p_out = pid->params.kp * error;
    pid->i_out = pid->params.ki * pid->integral;
    pid->d_out = pid->params.kd * derivative;
    pid->output = pid->p_out + pid->i_out + pid->d_out;
    if (pid_has_positive_limit(pid->params.output_limit)) {
        pid->output = App_Math_ClampFloat(pid->output, -pid->params.output_limit, pid->params.output_limit);
    }
    }
#endif

    pid->last_error = error;
    pid->has_last_error = true;
    return pid->output;
}
