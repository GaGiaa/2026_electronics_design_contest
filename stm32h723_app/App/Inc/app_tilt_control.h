#ifndef APP_TILT_CONTROL_H
#define APP_TILT_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "pid.h"

typedef enum {
    APP_TILT_STATE_DISABLED = 0U,
    APP_TILT_STATE_ACTIVE = 1U,
    APP_TILT_STATE_HOLD = 2U,
    APP_TILT_STATE_FAULT = 3U,
} app_tilt_state_t;

typedef enum {
    APP_TILT_FAULT_NONE = 0U,
    APP_TILT_FAULT_INVALID_CONFIG = 1U,
    APP_TILT_FAULT_BALANCE_NOT_HOMED = 2U,
    APP_TILT_FAULT_MOTOR_FEEDBACK = 3U,
    APP_TILT_FAULT_IMU_STALE = 4U,
    APP_TILT_FAULT_INVALID_INPUT = 5U,
    APP_TILT_FAULT_ZERO_NOT_CAPTURED = 6U,
} app_tilt_fault_t;

typedef struct {
    float position_min_deg;
    float position_max_deg;
    uint32_t sample_period_ms;
    uint32_t sample_max_age_ms;
    float pitch_to_tilt_sign;
    float derivative_filter_N;
    PID_Position_Param_Config pid_params;
} app_tilt_control_config_t;

typedef struct {
    uint32_t now_ms;
    bool enabled;
    float target_tilt_deg;
    bool capture_zero_request;
    bool balance_zero_valid;
    bool motor_feedback_valid;
    float motor_feedback_position_deg;
    bool imu_online;
    bool imu_valid;
    float imu_pitch_deg;
    uint32_t imu_sample_count;
    uint32_t imu_sample_age_ms;
} app_tilt_control_input_t;

typedef struct {
    app_tilt_state_t state;
    app_tilt_fault_t fault;
    bool capture_zero_consumed;
    bool zero_captured_valid;
    bool new_imu_sample;
    bool motor_target_clamped;
    float captured_zero_deg;
    float raw_pitch_deg;
    float tilt_deg;
    float target_tilt_deg;
    float error_deg;
    float pid_rate_deg_s;
    float pid_p_out_deg_s;
    float pid_i_out_deg_s;
    float pid_d_out_deg_s;
    float pid_integral;
    float measured_tilt_rate_deg_s;
    float filtered_tilt_rate_deg_s;
    float motor_target_position_deg;
} app_tilt_control_output_t;

typedef struct {
    float integral;
    float last_error;
    float last_tilt_deg;
    float measured_tilt_rate_deg_s;
    float filtered_tilt_rate_deg_s;
    float p_out;
    float i_out;
    float d_out;
    float output;
    bool has_measurement_history;
    bool integral_advanced;
} app_tilt_advanced_pid_t;

typedef struct {
    app_tilt_control_config_t config;
    app_tilt_advanced_pid_t pid;
    app_tilt_state_t state;
    app_tilt_fault_t fault;
    float captured_zero_deg;
    float motor_target_position_deg;
    uint32_t last_imu_sample_count;
    bool target_valid;
    bool zero_captured_valid;
    bool last_imu_sample_valid;
    bool enable_seen;
    bool capture_zero_request_seen;
} app_tilt_control_t;

void app_tilt_control_init(app_tilt_control_t *control,
                           const app_tilt_control_config_t *config);
void app_tilt_control_step(app_tilt_control_t *control,
                           const app_tilt_control_input_t *input,
                           app_tilt_control_output_t *output);

#endif /* APP_TILT_CONTROL_H */
