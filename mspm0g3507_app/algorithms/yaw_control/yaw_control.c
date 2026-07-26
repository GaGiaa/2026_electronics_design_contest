#include "yaw_control.h"

#include <math.h>
#include <stddef.h>

#include "algorithms/pid/app_math.h"
#include "config/app_config.h"
#include "config/crsf_config.h"

#define YAW_CONTROL_DEFAULT_MAX_TURN_SPEED_MM_PER_S 700.0f
#define YAW_CONTROL_DEFAULT_MAX_WHEEL_SPEED_MM_PER_S CRSF_MAX_SPEED_MM_PER_S
#define YAW_CONTROL_DEFAULT_TURN_SIGN -1.0f
#define YAW_CONTROL_DEFAULT_KP 15.0f

static const PID_Position_Param_Config g_default_pid_params = {
    .kp = YAW_CONTROL_DEFAULT_KP,
    .ki = 0.0f,
    .kd = 0.0f,
    .output_limit = YAW_CONTROL_DEFAULT_MAX_TURN_SPEED_MM_PER_S,
    .deadband = 0.1f,
};

volatile yaw_control_debug_t g_yaw_control_debug = {
    .use_pid_override = false,
    .target_yaw_deg = 0.0f,
    .pid_params = {
        .kp = YAW_CONTROL_DEFAULT_KP,
        .ki = 0.0f,
        .kd = 0.0f,
        .output_limit = YAW_CONTROL_DEFAULT_MAX_TURN_SPEED_MM_PER_S,
        .deadband = 0.0f,
    },
    .max_turn_speed_mm_per_s = YAW_CONTROL_DEFAULT_MAX_TURN_SPEED_MM_PER_S,
    .max_wheel_speed_mm_per_s = YAW_CONTROL_DEFAULT_MAX_WHEEL_SPEED_MM_PER_S,
    .turn_sign = YAW_CONTROL_DEFAULT_TURN_SIGN,
};

typedef struct {
    float target_yaw_deg;
    PID_Position_Param_Config pid_params;
    float max_turn_speed_mm_per_s;
    float max_wheel_speed_mm_per_s;
    float turn_sign;
} yaw_control_config_t;

static bool pid_params_are_valid(const PID_Position_Param_Config *params)
{
    return params != NULL && isfinite(params->kp) && isfinite(params->ki) &&
           isfinite(params->kd) && isfinite(params->output_limit) &&
           isfinite(params->deadband) && params->kp >= 0.0f &&
           params->ki >= 0.0f && params->kd >= 0.0f &&
           params->output_limit > 0.0f && params->deadband >= 0.0f;
}

static float wrap_angle_deg(float angle_deg)
{
    float wrapped;

    wrapped = fmodf(angle_deg + 180.0f, 360.0f);
    if (wrapped < 0.0f) {
        wrapped += 360.0f;
    }
    return wrapped - 180.0f;
}

static void yaw_control_config_set_defaults(yaw_control_config_t *config)
{
    if (config == NULL) {
        return;
    }

    config->target_yaw_deg = 0.0f;
    config->pid_params = g_default_pid_params;
    config->max_turn_speed_mm_per_s =
        YAW_CONTROL_DEFAULT_MAX_TURN_SPEED_MM_PER_S;
    config->max_wheel_speed_mm_per_s =
        YAW_CONTROL_DEFAULT_MAX_WHEEL_SPEED_MM_PER_S;
    config->turn_sign = YAW_CONTROL_DEFAULT_TURN_SIGN;
}

static bool yaw_control_config_read(yaw_control_config_t *config)
{
    yaw_control_debug_t debug;

    if (config == NULL) {
        return false;
    }
    yaw_control_config_set_defaults(config);
    debug = g_yaw_control_debug;
    if (!isfinite(debug.target_yaw_deg) ||
        !isfinite(debug.max_turn_speed_mm_per_s) ||
        debug.max_turn_speed_mm_per_s <= 0.0f ||
        !isfinite(debug.max_wheel_speed_mm_per_s) ||
        debug.max_wheel_speed_mm_per_s <= 0.0f ||
        !isfinite(debug.turn_sign) || debug.turn_sign == 0.0f ||
        (debug.use_pid_override &&
         !pid_params_are_valid(&debug.pid_params))) {
        return false;
    }

    config->target_yaw_deg = wrap_angle_deg(debug.target_yaw_deg);
    config->pid_params = debug.use_pid_override ? debug.pid_params :
                                                   g_default_pid_params;
    config->max_turn_speed_mm_per_s = debug.max_turn_speed_mm_per_s;
    config->max_wheel_speed_mm_per_s = debug.max_wheel_speed_mm_per_s;
    config->turn_sign = debug.turn_sign < 0.0f ? -1.0f : 1.0f;
    return true;
}

static void output_clear(yaw_control_output_t *output)
{
    if (output != NULL) {
        *output = (yaw_control_output_t){0};
    }
}

static void mix_targets(float base_speed, float turn_speed,
                        float max_wheel_speed,
                        float targets[BOARD_MOTOR_COUNT])
{
    float left;
    float right;
    float maximum;
    uint32_t wheel;

    left = base_speed + turn_speed;
    right = base_speed - turn_speed;
    maximum = fmaxf(fabsf(left), fabsf(right));
    if (maximum > max_wheel_speed) {
        const float scale = max_wheel_speed / maximum;

        left *= scale;
        right *= scale;
    }

    for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
        targets[wheel] = 0.0f;
    }
    targets[BOARD_MOTOR_FRONT_LEFT] = left;
    targets[BOARD_MOTOR_REAR_LEFT] = left;
    targets[BOARD_MOTOR_FRONT_RIGHT] = right;
    targets[BOARD_MOTOR_REAR_RIGHT] = right;
}

static void copy_pid_status(const yaw_control_state_t *state,
                            yaw_control_output_t *output)
{
    output->pid_p_out = state->pid.p_out;
    output->pid_i_out = state->pid.i_out;
    output->pid_d_out = state->pid.d_out;
    output->pid_output = state->pid.output;
}

void yaw_control_init(yaw_control_state_t *state)
{
    yaw_control_config_t config;

    if (state == NULL) {
        return;
    }
    (void)yaw_control_config_read(&config);
    PID_Position_Init(&state->pid, &config.pid_params,
                      (float)APP_YAW_CONTROL_INTERVAL_MS / 1000.0f);
    state->has_position_update = false;
    state->has_last_yaw_error = false;
    state->last_position_update_ms = 0U;
    state->last_yaw_error_deg = 0.0f;
    state->last_turn_speed_mm_per_s = 0.0f;
}

void yaw_control_reset(yaw_control_state_t *state)
{
    if (state == NULL) {
        return;
    }
    PID_Position_Reset(&state->pid);
    state->has_position_update = false;
    state->has_last_yaw_error = false;
    state->last_position_update_ms = 0U;
    state->last_yaw_error_deg = 0.0f;
    state->last_turn_speed_mm_per_s = 0.0f;
}

void yaw_control_step(yaw_control_state_t *state,
                      const yaw_control_input_t *input,
                      yaw_control_output_t *output)
{
    yaw_control_config_t config;
    float yaw_error_deg;

    if ((state == NULL) || (input == NULL) || (output == NULL)) {
        return;
    }

    output_clear(output);
    if (!yaw_control_config_read(&config) || !input->feedback_valid ||
        !isfinite(input->feedback_yaw_deg) ||
        !isfinite(input->base_speed_mm_per_s)) {
        yaw_control_reset(state);
        return;
    }

    yaw_error_deg = wrap_angle_deg(config.target_yaw_deg -
                                   wrap_angle_deg(input->feedback_yaw_deg));
    output->yaw_valid = true;
    output->yaw_error_deg = yaw_error_deg;
    if (!state->has_position_update ||
        (uint32_t)(input->now_ms - state->last_position_update_ms) >=
            APP_YAW_CONTROL_INTERVAL_MS) {
        if (state->has_last_yaw_error &&
            fabsf(yaw_error_deg - state->last_yaw_error_deg) > 180.0f) {
            PID_Position_Reset(&state->pid);
        }
        state->pid.params = config.pid_params;
        state->last_turn_speed_mm_per_s = App_Math_ClampFloat(
            config.turn_sign *
                PID_Position_Calc(&state->pid, yaw_error_deg, 0.0f),
            -config.max_turn_speed_mm_per_s,
            config.max_turn_speed_mm_per_s);
        state->last_position_update_ms = input->now_ms;
        state->has_position_update = true;
        state->last_yaw_error_deg = yaw_error_deg;
        state->has_last_yaw_error = true;
    }

    output->turn_speed_mm_per_s = state->last_turn_speed_mm_per_s;
    copy_pid_status(state, output);
    mix_targets(input->base_speed_mm_per_s, state->last_turn_speed_mm_per_s,
                config.max_wheel_speed_mm_per_s,
                output->wheel_targets_mm_per_s);
}
