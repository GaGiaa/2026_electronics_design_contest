#include "line_control.h"

#include <math.h>
#include <stddef.h>

#include "algorithms/pid/app_math.h"
#include "config/crsf_config.h"

#define LINE_CONTROL_DEFAULT_MAX_TURN_SPEED_MM_PER_S 300.0f
#define LINE_CONTROL_DEFAULT_MAX_WHEEL_SPEED_MM_PER_S \
    CRSF_MAX_SPEED_MM_PER_S
#define LINE_CONTROL_DEFAULT_TURN_SIGN -1.0f
#define LINE_CONTROL_DEFAULT_STRENGTH_ENTER 10000U
#define LINE_CONTROL_DEFAULT_STRENGTH_EXIT 5000U
#define LINE_CONTROL_DEFAULT_LOST_TIMEOUT_MS 100U
#define LINE_CONTROL_DEFAULT_KP 0.4f

static const PID_Position_Param_Config g_default_pid_params = {
    .kp = LINE_CONTROL_DEFAULT_KP,
    .ki = 0.0f,
    .kd = 0.0f,
    .output_limit = LINE_CONTROL_DEFAULT_MAX_TURN_SPEED_MM_PER_S,
    .deadband = 0.0f,
};

volatile line_control_debug_t g_line_control_debug = {
    .use_pid_override = false,
    .pid_params = {
        .kp = LINE_CONTROL_DEFAULT_KP,
        .ki = 0.0f,
        .kd = 0.0f,
        .output_limit = LINE_CONTROL_DEFAULT_MAX_TURN_SPEED_MM_PER_S,
        .deadband = 0.0f,
    },
    .max_turn_speed_mm_per_s = LINE_CONTROL_DEFAULT_MAX_TURN_SPEED_MM_PER_S,
    .max_wheel_speed_mm_per_s = LINE_CONTROL_DEFAULT_MAX_WHEEL_SPEED_MM_PER_S,
    .turn_sign = LINE_CONTROL_DEFAULT_TURN_SIGN,
    .line_strength_enter = LINE_CONTROL_DEFAULT_STRENGTH_ENTER,
    .line_strength_exit = LINE_CONTROL_DEFAULT_STRENGTH_EXIT,
    .lost_line_timeout_ms = LINE_CONTROL_DEFAULT_LOST_TIMEOUT_MS,
};

typedef struct {
    PID_Position_Param_Config pid_params;
    float max_turn_speed_mm_per_s;
    float max_wheel_speed_mm_per_s;
    float turn_sign;
    uint32_t line_strength_enter;
    uint32_t line_strength_exit;
    uint32_t lost_line_timeout_ms;
} line_control_config_t;

static bool pid_params_are_valid(const PID_Position_Param_Config *params)
{
    return params != NULL && isfinite(params->kp) && isfinite(params->ki) &&
           isfinite(params->kd) && isfinite(params->output_limit) &&
           isfinite(params->deadband) && params->kp >= 0.0f &&
           params->ki >= 0.0f && params->kd >= 0.0f &&
           params->output_limit > 0.0f && params->deadband >= 0.0f;
}

static void line_control_config_read(line_control_config_t *config)
{
    line_control_debug_t debug;

    if (config == NULL) {
        return;
    }

    debug = g_line_control_debug;
    config->pid_params = debug.use_pid_override &&
                                 pid_params_are_valid(&debug.pid_params) ?
                             debug.pid_params :
                             g_default_pid_params;
    config->max_turn_speed_mm_per_s =
        isfinite(debug.max_turn_speed_mm_per_s) &&
                debug.max_turn_speed_mm_per_s > 0.0f ?
            debug.max_turn_speed_mm_per_s :
            LINE_CONTROL_DEFAULT_MAX_TURN_SPEED_MM_PER_S;
    config->max_wheel_speed_mm_per_s =
        isfinite(debug.max_wheel_speed_mm_per_s) &&
                debug.max_wheel_speed_mm_per_s > 0.0f ?
            debug.max_wheel_speed_mm_per_s :
            LINE_CONTROL_DEFAULT_MAX_WHEEL_SPEED_MM_PER_S;
    config->turn_sign = isfinite(debug.turn_sign) && debug.turn_sign != 0.0f ?
                            (debug.turn_sign < 0.0f ? -1.0f : 1.0f) :
                            LINE_CONTROL_DEFAULT_TURN_SIGN;
    config->line_strength_enter = debug.line_strength_enter != 0U ?
                                      debug.line_strength_enter :
                                      LINE_CONTROL_DEFAULT_STRENGTH_ENTER;
    config->line_strength_exit =
        debug.line_strength_exit != 0U &&
                debug.line_strength_exit < config->line_strength_enter ?
                                     debug.line_strength_exit :
                                     LINE_CONTROL_DEFAULT_STRENGTH_EXIT;
    config->lost_line_timeout_ms = debug.lost_line_timeout_ms != 0U ?
                                       debug.lost_line_timeout_ms :
                                       LINE_CONTROL_DEFAULT_LOST_TIMEOUT_MS;
}

static void output_clear(line_control_output_t *output)
{
    if (output != NULL) {
        *output = (line_control_output_t){0};
    }
}

static bool sample_is_valid(const line_control_state_t *state,
                            const line_control_input_t *input,
                            const line_control_config_t *config)
{
    uint32_t threshold;

    if ((state == NULL) || (input == NULL) || (config == NULL) ||
        input->sequence == 0U || input->adc_timeout_mask != 0U) {
        return false;
    }

    threshold = state->line_valid ? config->line_strength_exit :
                                    config->line_strength_enter;
    return input->line_strength >= threshold;
}

static void mix_targets(float base_speed, float turn_speed,
                        float max_wheel_speed,
                        float targets[BOARD_MOTOR_COUNT])
{
    float left;
    float right;
    float maximum;
    uint32_t wheel;

    if (targets == NULL) {
        return;
    }

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

static void copy_pid_status(const line_control_state_t *state,
                            line_control_output_t *output)
{
    output->pid_p_out = state->pid.p_out;
    output->pid_i_out = state->pid.i_out;
    output->pid_d_out = state->pid.d_out;
    output->pid_output = state->pid.output;
}

void line_control_init(line_control_state_t *state)
{
    line_control_config_t config;

    if (state == NULL) {
        return;
    }
    line_control_config_read(&config);
    PID_Position_Init(&state->pid, &config.pid_params, 0.01f);
    state->has_valid_line = false;
    state->line_valid = false;
    state->last_sequence = 0U;
    state->last_valid_time_ms = 0U;
    state->last_turn_speed_mm_per_s = 0.0f;
}

void line_control_reset(line_control_state_t *state)
{
    if (state == NULL) {
        return;
    }
    PID_Position_Reset(&state->pid);
    state->has_valid_line = false;
    state->line_valid = false;
    state->last_sequence = 0U;
    state->last_valid_time_ms = 0U;
    state->last_turn_speed_mm_per_s = 0.0f;
}

void line_control_step(line_control_state_t *state,
                       const line_control_input_t *input,
                       line_control_output_t *output)
{
    line_control_config_t config;
    bool new_sample;
    bool valid_sample;
    uint32_t elapsed_ms = 0U;

    if ((state == NULL) || (input == NULL) || (output == NULL)) {
        return;
    }

    output_clear(output);
    line_control_config_read(&config);
    new_sample = input->sequence != state->last_sequence;
    if (new_sample) {
        state->last_sequence = input->sequence;
        valid_sample = sample_is_valid(state, input, &config);
        state->line_valid = valid_sample;
        if (valid_sample) {
            state->has_valid_line = true;
            state->last_valid_time_ms = input->now_ms;
            state->pid.params = config.pid_params;
            state->last_turn_speed_mm_per_s =
                App_Math_ClampFloat(
                    config.turn_sign *
                        PID_Position_Calc(&state->pid, 0.0f,
                                          (float)input->line_error),
                    -config.max_turn_speed_mm_per_s,
                    config.max_turn_speed_mm_per_s);
        }
    }

    if (!state->has_valid_line) {
        return;
    }

    elapsed_ms = (uint32_t)(input->now_ms - state->last_valid_time_ms);
    if (elapsed_ms > config.lost_line_timeout_ms ||
        !isfinite(input->base_speed_mm_per_s)) {
        line_control_reset(state);
        return;
    }

    output->line_valid = state->line_valid;
    output->lost_ms = state->line_valid ? 0U : elapsed_ms;
    output->turn_speed_mm_per_s = state->last_turn_speed_mm_per_s;
    copy_pid_status(state, output);
    mix_targets(input->base_speed_mm_per_s, state->last_turn_speed_mm_per_s,
                config.max_wheel_speed_mm_per_s,
                output->wheel_targets_mm_per_s);
}
