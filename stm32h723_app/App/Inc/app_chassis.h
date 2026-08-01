#ifndef APP_CHASSIS_H
#define APP_CHASSIS_H

#include <stdbool.h>
#include <stdint.h>

#include "app_crsf.h"
#include "app_config.h"
#include "pid.h"

typedef enum {
    APP_CHASSIS_MODE_TASK_MENU = 0U,
    APP_CHASSIS_MODE_REMOTE_IDLE = 1U,
    APP_CHASSIS_MODE_REMOTE_MANUAL = 2U,
    APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW = 3U,
    APP_CHASSIS_MODE_STOP = APP_CHASSIS_MODE_REMOTE_IDLE,
    APP_CHASSIS_MODE_MANUAL = APP_CHASSIS_MODE_REMOTE_MANUAL,
    APP_CHASSIS_MODE_LINE_FOLLOW = APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW
} app_chassis_mode_t;

typedef struct {
    bool manual_active;
    app_chassis_mode_t mode;
    float forward_normalized;
    float turn_normalized;
    float base_speed_mm_s;
    float left_target_rpm;
    float right_target_rpm;
    float third_motor_target_rpm;
} app_chassis_command_t;

typedef struct {
    uint32_t last_button_mask;
    bool previous_se_pressed;
    bool initialized;
} app_chassis_control_state_t;

typedef struct {
    app_chassis_command_t chassis;
    app_chassis_mode_t mode;
    bool remote_takeover;
    bool buttons_enabled;
    bool se_pressed;
    uint32_t sb_state;
    uint32_t sc_state;
    uint32_t selected_task;
    bool task_request_available;
    bool confirm_button_pressed;
} app_chassis_control_output_t;

void app_chassis_mix(const app_crsf_input_t *input, uint32_t now_ms, app_chassis_command_t *command);
void app_chassis_control_init(app_chassis_control_state_t *state);
void app_chassis_control_step(app_chassis_control_state_t *state,
                              const app_crsf_input_t *input,
                              uint32_t button_stable_high_mask,
                              uint32_t now_ms,
                              app_chassis_control_output_t *output);

#endif
