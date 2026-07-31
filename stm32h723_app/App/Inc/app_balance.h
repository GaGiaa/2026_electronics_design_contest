#ifndef APP_BALANCE_H
#define APP_BALANCE_H

#include <stdbool.h>
#include <stdint.h>

#include "pid.h"

typedef enum {
    APP_BALANCE_STATE_WAIT_FEEDBACK = 0U,
    APP_BALANCE_STATE_HOME_SEEK = 1U,
    APP_BALANCE_STATE_HOME_CONFIRM = 2U,
    APP_BALANCE_STATE_HOMED_IDLE = 3U,
    APP_BALANCE_STATE_POSITION = 4U,
    APP_BALANCE_STATE_FAULT = 5U,
} app_balance_state_t;

typedef enum {
    APP_BALANCE_FAULT_NONE = 0U,
    APP_BALANCE_FAULT_INVALID_CONFIG = 1U,
    APP_BALANCE_FAULT_FEEDBACK_LOST = 2U,
    APP_BALANCE_FAULT_HOME_TIMEOUT = 3U,
    APP_BALANCE_FAULT_INVALID_TARGET = 4U,
} app_balance_fault_t;

typedef struct {
    float home_search_output_speed_rpm;
    float home_current_limit_a;
    float home_stall_speed_rpm;
    float home_stall_current_ratio;
    uint32_t home_confirm_ms;
    uint32_t home_timeout_ms;
    float position_min_deg;
    float position_active_min_deg;
    float position_max_deg;
    uint32_t position_period_ms;
    PID_Incremental_Param_Config home_speed_params;
    PID_Incremental_Param_Config position_speed_params;
    PID_Position_Param_Config position_params;
} app_balance_config_t;

typedef struct {
    uint32_t now_ms;
    bool feedback_valid;
    float feedback_position_deg;
    float feedback_output_speed_rpm;
    float feedback_current_a;
    float requested_target_position_deg;
    bool rehome_request;
} app_balance_step_input_t;

typedef struct {
    app_balance_state_t state;
    app_balance_fault_t fault;
    bool zero_valid;
    bool target_clamped;
    bool rehome_request_consumed;
    float zero_offset_deg;
    float feedback_position_deg;
    float requested_target_position_deg;
    float active_target_position_deg;
    float target_output_speed_rpm;
    float commanded_current_a;
} app_balance_step_output_t;

typedef struct {
    app_balance_config_t config;
    PID_Incremental speed_pid;
    PID_Position position_pid;
    app_balance_state_t state;
    app_balance_fault_t fault;
    float zero_offset_deg;
    float target_output_speed_rpm;
    uint32_t home_started_ms;
    uint32_t stall_started_ms;
    uint32_t position_last_update_ms;
    bool zero_valid;
    bool rehome_request_seen;
} app_balance_t;

void app_balance_init(app_balance_t *balance, const app_balance_config_t *config);
void app_balance_step(app_balance_t *balance,
                      const app_balance_step_input_t *input,
                      app_balance_step_output_t *output);

#endif
