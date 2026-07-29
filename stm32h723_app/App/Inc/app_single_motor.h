#ifndef APP_SINGLE_MOTOR_H
#define APP_SINGLE_MOTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "pid.h"

typedef struct {
    uint32_t enable;
    uint32_t control_mode;
    uint32_t selected_id;
    uint32_t default_id;
    float target_output_speed_rpm;
    PID_Incremental_Param_Config params;
    float target_position_deg;
    float feedback_position_deg;
    PID_Position_Param_Config position_params;
    bool position_reference_valid;
    bool position_update_due;
    bool configuration_changed;
    bool feedback_valid;
    uint32_t feedback_age_ms;
    float feedback_output_speed_rpm;
} app_single_motor_step_input_t;

typedef struct {
    bool active;
    bool reset_pid;
    uint32_t selected_id;
    uint32_t safety_reason;
    float target_output_speed_rpm;
    float commanded_current_A;
    float raw_output_A;
    float pid_output_A;
    float pid_p_out_A;
    float pid_i_out_A;
    float pid_d_out_A;
    float position_output_rpm;
    float position_p_out_rpm;
    float position_i_out_rpm;
    float position_d_out_rpm;
} app_single_motor_step_output_t;

enum {
    APP_SINGLE_MOTOR_CONTROL_MODE_SPEED = 0U,
    APP_SINGLE_MOTOR_CONTROL_MODE_POSITION = 1U,
};

enum {
    APP_SINGLE_MOTOR_SAFETY_OK = 0U,
    APP_SINGLE_MOTOR_SAFETY_DISABLED = 1U,
    APP_SINGLE_MOTOR_SAFETY_FEEDBACK = 2U,
    APP_SINGLE_MOTOR_SAFETY_TARGET = 3U,
    APP_SINGLE_MOTOR_SAFETY_PARAMS = 4U,
    APP_SINGLE_MOTOR_SAFETY_CONFIGURATION_CHANGED = 5U,
    APP_SINGLE_MOTOR_SAFETY_CONTROL_MODE = 6U,
    APP_SINGLE_MOTOR_SAFETY_POSITION_REFERENCE = 7U,
    APP_SINGLE_MOTOR_SAFETY_POSITION_TARGET = 8U,
};

uint32_t app_single_motor_sanitize_id(uint32_t requested_id, uint32_t default_id);
float app_single_motor_sanitize_max_output_speed_rpm(float requested_limit);
bool app_single_motor_step(PID_Incremental *speed_pid,
                           PID_Position *position_pid,
                           const app_single_motor_step_input_t *input,
                           uint32_t feedback_timeout_ms,
                           float max_target_output_speed_rpm,
                           float max_command_current_A,
                           app_single_motor_step_output_t *output);

#endif
