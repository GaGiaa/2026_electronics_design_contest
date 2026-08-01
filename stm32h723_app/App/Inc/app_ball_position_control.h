#ifndef APP_BALL_POSITION_CONTROL_H
#define APP_BALL_POSITION_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "pid.h"

#define APP_BALL_POSITION_CONTROL_PERIOD_MS (25U)

typedef enum {
    APP_BALL_POSITION_STATE_DISABLED = 0U,
    APP_BALL_POSITION_STATE_ACTIVE = 1U,
    APP_BALL_POSITION_STATE_HOLD = 2U,
    APP_BALL_POSITION_STATE_FAULT = 3U,
} app_ball_position_control_state_t;

typedef enum {
    APP_BALL_POSITION_FAULT_NONE = 0U,
    APP_BALL_POSITION_FAULT_INVALID_CONFIG = 1U,
    APP_BALL_POSITION_FAULT_NOT_CALIBRATED = 2U,
    APP_BALL_POSITION_FAULT_ID3_NOT_READY = 3U,
    APP_BALL_POSITION_FAULT_VISION_INVALID = 4U,
    APP_BALL_POSITION_FAULT_VISION_STALE = 5U,
    APP_BALL_POSITION_FAULT_INVALID_INPUT = 6U,
} app_ball_position_control_fault_t;

typedef struct {
    uint32_t period_ms;
    uint32_t max_age_ms;
    PID_Position_Param_Config pid_params;
    float output_limit_deg;
    float sign;
    float deadband_mm;
} app_ball_position_control_config_t;

typedef struct {
    uint32_t now_ms;
    bool enabled;
    float target_mm;
    float measured_mm;
    bool vision_valid;
    uint32_t vision_age_ms;
    bool calibration_ready;
    bool id3_ready;
} app_ball_position_control_input_t;

typedef struct {
    app_ball_position_control_state_t state;
    app_ball_position_control_fault_t fault;
    bool update_due;
    bool valid;
    bool reset;
    float error_mm;
    float p_out_deg;
    float i_out_deg;
    float d_out_deg;
    float output_deg;
    float target_tilt_deg;
    float integral;
} app_ball_position_control_output_t;

typedef struct {
    app_ball_position_control_config_t config;
    PID_Position pid;
    app_ball_position_control_state_t state;
    app_ball_position_control_fault_t fault;
    uint32_t last_update_ms;
    bool has_last_update;
} app_ball_position_control_t;

void app_ball_position_control_config_default(app_ball_position_control_config_t *config);
void app_ball_position_control_init(app_ball_position_control_t *control,
                                    const app_ball_position_control_config_t *config);
void app_ball_position_control_reset(app_ball_position_control_t *control);
void app_ball_position_control_step(app_ball_position_control_t *control,
                                    const app_ball_position_control_input_t *input,
                                    app_ball_position_control_output_t *output);

#endif /* APP_BALL_POSITION_CONTROL_H */
