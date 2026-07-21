#ifndef MOTOR_PID_H
#define MOTOR_PID_H

#include <stdbool.h>

#include "pid_config.h"

typedef struct {
    float kp;
    float ki;
    float kd;
    float output_limit;
    float deadband;
} PID_Incremental_Param_Config;

typedef struct {
    float kp;
    float ki;
    float kd;
    float output_limit;
    float deadband;
#if (PID_POSITION_CONFIG_VARIANT == PID_POSITION_VARIANT_ADVANCED)
    float I_Outlimit;
    float setpoint_weight_b;
    float setpoint_weight_c;
    float derivative_filter_N;
    float anti_windup_gain;
    float integral_separation_threshold;
    float output_filter_N;
#endif
} PID_Position_Param_Config;

typedef struct {
    PID_Incremental_Param_Config params;
    float dt_s;
    float error;
    float last_error;
    float prev_error;
    float p_out;
    float i_out;
    float d_out;
    float output;
} PID_Incremental;

typedef struct {
    PID_Position_Param_Config params;
    float dt_s;
    float integral;
    float last_error;
    float p_out;
    float i_out;
    float d_out;
    float output;
    bool has_last_error;
#if (PID_POSITION_CONFIG_VARIANT == PID_POSITION_VARIANT_ADVANCED)
    float last_target;
    float last_feedback;
    float filtered_derivative;
    float filtered_output;
    bool has_last_sample;
#endif
} PID_Position;

void PID_Incremental_Init(PID_Incremental *pid, const PID_Incremental_Param_Config *params, float dt_s);
void PID_Incremental_Reset(PID_Incremental *pid);
float PID_Incremental_Calc(PID_Incremental *pid, float target, float feedback);

void PID_Position_Init(PID_Position *pid, const PID_Position_Param_Config *params, float dt_s);
void PID_Position_Reset(PID_Position *pid);
float PID_Position_Calc(PID_Position *pid, float target, float feedback);

#endif
