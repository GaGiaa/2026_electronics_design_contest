#ifndef APP_SINGLE_MOTOR_H
#define APP_SINGLE_MOTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "pid.h"

typedef struct {
    uint32_t enable;
    uint32_t selected_id;
    uint32_t default_id;
    float target_speed_rpm;
    PID_Incremental_Param_Config params;
    bool configuration_changed;
    bool feedback_valid;
    uint32_t feedback_age_ms;
    int16_t feedback_speed_rpm;
} app_single_motor_step_input_t;

typedef struct {
    bool active;
    bool reset_pid;
    uint32_t selected_id;
    uint32_t safety_reason;
    int16_t commanded_current;
    float raw_output;
    float pid_output;
    float pid_p_out;
    float pid_i_out;
    float pid_d_out;
} app_single_motor_step_output_t;

enum {
    APP_SINGLE_MOTOR_SAFETY_OK = 0U,
    APP_SINGLE_MOTOR_SAFETY_DISABLED = 1U,
    APP_SINGLE_MOTOR_SAFETY_FEEDBACK = 2U,
    APP_SINGLE_MOTOR_SAFETY_TARGET = 3U,
    APP_SINGLE_MOTOR_SAFETY_PARAMS = 4U,
    APP_SINGLE_MOTOR_SAFETY_CONFIGURATION_CHANGED = 5U,
};

uint32_t app_single_motor_sanitize_id(uint32_t requested_id, uint32_t default_id);
bool app_single_motor_step(PID_Incremental *pid,
                           const app_single_motor_step_input_t *input,
                           uint32_t feedback_timeout_ms,
                           float max_target_speed_rpm,
                           float max_command_current,
                           app_single_motor_step_output_t *output);

#endif
