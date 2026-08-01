#ifndef APP_BALL_POSITION_CONTROL_H
#define APP_BALL_POSITION_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "pid.h"

#define APP_BALL_POSITION_CONTROL_PERIOD_MS (25U)
#define APP_BALL_POSITION_HOLD_MAP_POINT_COUNT (3U)

typedef enum {
    APP_BALL_POSITION_STATE_DISABLED = 0U,
    APP_BALL_POSITION_STATE_ACTIVE = 1U,
    APP_BALL_POSITION_STATE_HOLD = 2U,
    APP_BALL_POSITION_STATE_FAULT = 3U,
} app_ball_position_control_state_t;

typedef enum {
    APP_BALL_POSITION_FAULT_NONE = 0U,
    APP_BALL_POSITION_FAULT_INVALID_CONFIG = 1U,
    APP_BALL_POSITION_FAULT_ID3_NOT_READY = 2U,
    APP_BALL_POSITION_FAULT_VISION_INVALID = 3U,
    APP_BALL_POSITION_FAULT_VISION_STALE = 4U,
    APP_BALL_POSITION_FAULT_INVALID_INPUT = 5U,
} app_ball_position_control_fault_t;

typedef struct {
    uint32_t period_ms;
    uint32_t max_age_ms;
    float safe_motor_position_deg;
    PID_Position_Param_Config pid_params;
    float output_limit_deg;
    float sign;
    float deadband_mm;
    float hold_position_mm[APP_BALL_POSITION_HOLD_MAP_POINT_COUNT];
    float hold_motor_position_deg[APP_BALL_POSITION_HOLD_MAP_POINT_COUNT];
    float engage_error_mm;
    float release_error_mm;
} app_ball_position_control_config_t;

typedef struct {
    uint32_t now_ms;
    bool enabled;
    float target_mm;
    float measured_mm;
    bool vision_valid;
    uint32_t vision_age_ms;
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
    float pid_offset_deg;
    float integral;
    bool drive_active;
    float hold_motor_position_deg;
    float target_motor_position_deg;
} app_ball_position_control_output_t;

typedef struct {
    app_ball_position_control_config_t config;
    PID_Position pid;
    app_ball_position_control_state_t state;
    app_ball_position_control_fault_t fault;
    uint32_t last_update_ms;
    bool has_last_update;
    bool drive_active;
    float hold_motor_position_deg;
    float target_motor_position_deg;
} app_ball_position_control_t;

void app_ball_position_control_config_default(app_ball_position_control_config_t *config);
void app_ball_position_control_init(app_ball_position_control_t *control,
                                    const app_ball_position_control_config_t *config);
void app_ball_position_control_reset(app_ball_position_control_t *control);
void app_ball_position_control_step(app_ball_position_control_t *control,
                                    const app_ball_position_control_input_t *input,
                                    app_ball_position_control_output_t *output);

#endif /* APP_BALL_POSITION_CONTROL_H */
