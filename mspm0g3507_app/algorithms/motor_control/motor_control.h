#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <stdbool.h>

#include "drivers/encoder/board_encoder.h"
#include "pid.h"

typedef enum {
    MOTOR_CONTROL_DEBUG_MODE_STOP = 0U,
    MOTOR_CONTROL_DEBUG_MODE_PWM,
    MOTOR_CONTROL_DEBUG_MODE_SPEED,
} motor_control_debug_mode_t;

typedef struct {
    float target_speed_mm_per_s;
    float instant_feedback_speed_mm_per_s;
    float feedback_speed_mm_per_s;
    float pid_p_out;
    float pid_i_out;
    float pid_d_out;
    float pid_output;
    float output_duty_percent;
} motor_control_wheel_status_t;

typedef struct {
    bool enable;
    motor_control_debug_mode_t mode;
    board_motor_wheel_t wheel;
    bool use_speed_pid_override;
    float target_duty_percent;
    float target_speed_mm_per_s;
    float instant_feedback_speed_mm_per_s;
    PID_Incremental_Param_Config speed_pid_params;
    float feedback_speed_mm_per_s;
    float pid_p_out;
    float pid_i_out;
    float pid_d_out;
    float pid_output;
    float output_duty_percent;
} motor_control_debug_t;

extern volatile float g_motor_speed_targets_mm_s[BOARD_MOTOR_COUNT];
extern volatile motor_control_wheel_status_t g_motor_control_status[BOARD_MOTOR_COUNT];
extern volatile motor_control_debug_t g_motor_debug;

void motor_control_init(void);
void motor_control_step(const board_encoder_sample_t samples[BOARD_MOTOR_COUNT]);
float motor_control_get_output_duty_percent(board_motor_wheel_t wheel);

#endif
