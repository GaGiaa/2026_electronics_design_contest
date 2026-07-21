#include "motor_control.h"

#include <math.h>
#include <stddef.h>

#include "app_math.h"

#define MOTOR_CONTROL_DT_S 0.01f
#define MOTOR_CONTROL_MAX_DUTY_PERCENT 100.0f

static const PID_Incremental_Param_Config g_default_speed_pid_params = {
    .kp = 0.1f,
    .ki = 0.0f,
    .kd = 0.0f,
    .output_limit = MOTOR_CONTROL_MAX_DUTY_PERCENT,
    .deadband = 0.0f,
};

static const PID_Position_Param_Config g_default_position_pid_params = {
    .kp = 0.0f,
    .ki = 0.0f,
    .kd = 0.0f,
    .output_limit = MOTOR_CONTROL_MAX_DUTY_PERCENT,
    .deadband = 0.0f,
};

volatile float g_motor_speed_targets_mm_s[BOARD_MOTOR_COUNT];
volatile motor_control_wheel_status_t g_motor_control_status[BOARD_MOTOR_COUNT];
volatile motor_control_debug_t g_motor_debug = {
    .enable = false,
    .mode = MOTOR_CONTROL_DEBUG_MODE_STOP,
    .wheel = BOARD_MOTOR_FRONT_LEFT,
    .target_duty_percent = 0.0f,
    .target_speed_mm_per_s = 0.0f,
    .speed_pid_params = {
        .kp = 0.1f,
        .ki = 0.0f,
        .kd = 0.0f,
        .output_limit = MOTOR_CONTROL_MAX_DUTY_PERCENT,
        .deadband = 0.0f,
    },
};

static PID_Incremental g_speed_pids[BOARD_MOTOR_COUNT];
static PID_Position g_position_pids[BOARD_MOTOR_COUNT];
static motor_control_debug_mode_t g_last_debug_mode;
static board_motor_wheel_t g_last_debug_wheel;
static bool g_last_debug_enabled;

static bool debug_mode_is_valid(motor_control_debug_mode_t mode)
{
    return mode == MOTOR_CONTROL_DEBUG_MODE_STOP ||
           mode == MOTOR_CONTROL_DEBUG_MODE_PWM ||
           mode == MOTOR_CONTROL_DEBUG_MODE_SPEED;
}

static bool speed_pid_params_are_valid(const PID_Incremental_Param_Config *params)
{
    return params != NULL && isfinite(params->kp) && isfinite(params->ki) &&
           isfinite(params->kd) && isfinite(params->output_limit) &&
           isfinite(params->deadband) && params->kp >= 0.0f && params->ki >= 0.0f &&
           params->kd >= 0.0f && params->output_limit > 0.0f &&
           params->deadband >= 0.0f;
}

static float clamp_duty_percent(float duty_percent)
{
    if (!isfinite(duty_percent)) {
        return 0.0f;
    }

    return App_Math_ClampFloat(duty_percent, -MOTOR_CONTROL_MAX_DUTY_PERCENT,
                               MOTOR_CONTROL_MAX_DUTY_PERCENT);
}

static bool speed_command_is_stop(float target_speed, float feedback_speed)
{
    return !isfinite(target_speed) || !isfinite(feedback_speed) || target_speed == 0.0f;
}

static void reset_all_controllers(void)
{
    uint32_t wheel;

    for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
        PID_Incremental_Reset(&g_speed_pids[wheel]);
        PID_Position_Reset(&g_position_pids[wheel]);
    }
}

static void update_status(board_motor_wheel_t wheel, float target_speed,
                          float feedback_speed, float output_duty)
{
    g_motor_control_status[wheel].target_speed_mm_per_s = target_speed;
    g_motor_control_status[wheel].feedback_speed_mm_per_s = feedback_speed;
    g_motor_control_status[wheel].pid_p_out = g_speed_pids[wheel].p_out;
    g_motor_control_status[wheel].pid_i_out = g_speed_pids[wheel].i_out;
    g_motor_control_status[wheel].pid_d_out = g_speed_pids[wheel].d_out;
    g_motor_control_status[wheel].pid_output = g_speed_pids[wheel].output;
    g_motor_control_status[wheel].output_duty_percent = output_duty;
}

void motor_control_init(void)
{
    uint32_t wheel;

    for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
        g_motor_speed_targets_mm_s[wheel] = 0.0f;
        g_motor_control_status[wheel] = (motor_control_wheel_status_t){0};
        PID_Incremental_Init(&g_speed_pids[wheel], &g_default_speed_pid_params, MOTOR_CONTROL_DT_S);
        PID_Position_Init(&g_position_pids[wheel], &g_default_position_pid_params, MOTOR_CONTROL_DT_S);
    }
    g_motor_debug.enable = false;
    g_motor_debug.mode = MOTOR_CONTROL_DEBUG_MODE_STOP;
    g_motor_debug.wheel = BOARD_MOTOR_FRONT_LEFT;
    g_motor_debug.target_duty_percent = 0.0f;
    g_motor_debug.target_speed_mm_per_s = 0.0f;
    g_motor_debug.speed_pid_params = g_default_speed_pid_params;
    g_motor_debug.feedback_speed_mm_per_s = 0.0f;
    g_motor_debug.pid_p_out = 0.0f;
    g_motor_debug.pid_i_out = 0.0f;
    g_motor_debug.pid_d_out = 0.0f;
    g_motor_debug.pid_output = 0.0f;
    g_motor_debug.output_duty_percent = 0.0f;
    g_last_debug_enabled = false;
    g_last_debug_mode = MOTOR_CONTROL_DEBUG_MODE_STOP;
    g_last_debug_wheel = BOARD_MOTOR_FRONT_LEFT;
}

void motor_control_step(const board_encoder_sample_t samples[BOARD_MOTOR_COUNT])
{
    bool debug_active;
    bool debug_configuration_valid;
    bool debug_changed;
    uint32_t wheel;

    if (samples == NULL) {
        return;
    }

    debug_active = g_motor_debug.enable;
    debug_configuration_valid = debug_mode_is_valid(g_motor_debug.mode) &&
                                g_motor_debug.wheel < BOARD_MOTOR_COUNT &&
                                (g_motor_debug.mode != MOTOR_CONTROL_DEBUG_MODE_SPEED ||
                                 speed_pid_params_are_valid((const PID_Incremental_Param_Config *)&g_motor_debug.speed_pid_params));
    debug_changed = g_last_debug_enabled != debug_active ||
                    g_last_debug_mode != g_motor_debug.mode ||
                    g_last_debug_wheel != g_motor_debug.wheel;
    if (debug_changed) {
        reset_all_controllers();
    }

    for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
        const board_motor_wheel_t wheel_id = (board_motor_wheel_t)wheel;
        float target_speed = g_motor_speed_targets_mm_s[wheel];
        float output_duty;

        g_speed_pids[wheel].params = g_default_speed_pid_params;
        if (debug_changed) {
            output_duty = 0.0f;
            target_speed = 0.0f;
        } else if (debug_active) {
            if (!debug_configuration_valid || wheel_id != g_motor_debug.wheel) {
                PID_Incremental_Reset(&g_speed_pids[wheel]);
                output_duty = 0.0f;
                target_speed = 0.0f;
            } else if (g_motor_debug.mode == MOTOR_CONTROL_DEBUG_MODE_PWM) {
                PID_Incremental_Reset(&g_speed_pids[wheel]);
                output_duty = clamp_duty_percent(g_motor_debug.target_duty_percent);
                target_speed = 0.0f;
            } else if (g_motor_debug.mode == MOTOR_CONTROL_DEBUG_MODE_SPEED) {
                g_speed_pids[wheel].params = g_motor_debug.speed_pid_params;
                target_speed = g_motor_debug.target_speed_mm_per_s;
                if (speed_command_is_stop(target_speed, samples[wheel].speed_mm_per_s)) {
                    PID_Incremental_Reset(&g_speed_pids[wheel]);
                    output_duty = 0.0f;
                } else {
                    output_duty = clamp_duty_percent(PID_Incremental_Calc(&g_speed_pids[wheel], target_speed,
                                                                            samples[wheel].speed_mm_per_s));
                }
            } else {
                PID_Incremental_Reset(&g_speed_pids[wheel]);
                output_duty = 0.0f;
                target_speed = 0.0f;
            }
        } else {
            if (speed_command_is_stop(target_speed, samples[wheel].speed_mm_per_s)) {
                PID_Incremental_Reset(&g_speed_pids[wheel]);
                output_duty = 0.0f;
            } else {
                output_duty = clamp_duty_percent(PID_Incremental_Calc(&g_speed_pids[wheel], target_speed,
                                                                        samples[wheel].speed_mm_per_s));
            }
        }
        update_status(wheel_id, target_speed, samples[wheel].speed_mm_per_s, output_duty);
    }

    if (debug_active && debug_configuration_valid && g_motor_debug.wheel < BOARD_MOTOR_COUNT) {
        g_motor_debug.feedback_speed_mm_per_s =
            g_motor_control_status[g_motor_debug.wheel].feedback_speed_mm_per_s;
        g_motor_debug.pid_p_out = g_motor_control_status[g_motor_debug.wheel].pid_p_out;
        g_motor_debug.pid_i_out = g_motor_control_status[g_motor_debug.wheel].pid_i_out;
        g_motor_debug.pid_d_out = g_motor_control_status[g_motor_debug.wheel].pid_d_out;
        g_motor_debug.pid_output = g_motor_control_status[g_motor_debug.wheel].pid_output;
        g_motor_debug.output_duty_percent =
            g_motor_control_status[g_motor_debug.wheel].output_duty_percent;
    } else {
        g_motor_debug.feedback_speed_mm_per_s = 0.0f;
        g_motor_debug.pid_p_out = 0.0f;
        g_motor_debug.pid_i_out = 0.0f;
        g_motor_debug.pid_d_out = 0.0f;
        g_motor_debug.pid_output = 0.0f;
        g_motor_debug.output_duty_percent = 0.0f;
    }

    g_last_debug_enabled = debug_active;
    g_last_debug_mode = g_motor_debug.mode;
    g_last_debug_wheel = g_motor_debug.wheel;
}

float motor_control_get_output_duty_percent(board_motor_wheel_t wheel)
{
    if (wheel >= BOARD_MOTOR_COUNT) {
        return 0.0f;
    }

    return g_motor_control_status[wheel].output_duty_percent;
}
