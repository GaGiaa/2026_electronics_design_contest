#include "app_line_follow.h"

#include <stddef.h>

#include "app_config.h"
#include "app_math.h"

static float clamp_wheel_speed(float value)
{
    return App_Math_ClampFloat(value, -APP_H723_CHASSIS_MAX_OUTPUT_RPM,
                               APP_H723_CHASSIS_MAX_OUTPUT_RPM);
}

static float speed_mm_s_to_output_rpm(float speed_mm_s)
{
    return speed_mm_s * APP_H723_MM_S_TO_OUTPUT_RPM;
}

static void clear_output(app_line_follow_output_t *output)
{
    if (output == NULL) {
        return;
    }

    output->active = false;
    output->line_valid = false;
    output->line_position = 0.0f;
    output->turn_correction_mm_s = 0.0f;
    output->left_target_rpm = 0.0f;
    output->right_target_rpm = 0.0f;
    output->sequence = 0U;
}

static void make_output(const app_line_follow_state_t *state,
                        const app_line_follow_input_t *input,
                        app_line_follow_output_t *output)
{
    const float base_speed_mm_s = App_Math_ClampFloat(
        input->base_speed_mm_s, 0.0f, APP_H723_LINE_FOLLOW_MAX_SPEED_MM_S);
    const float left_speed_mm_s = base_speed_mm_s + state->turn_correction_mm_s;
    const float right_speed_mm_s = base_speed_mm_s - state->turn_correction_mm_s;

    output->active = true;
    output->line_valid = true;
    output->line_position = input->line_position;
    output->turn_correction_mm_s = state->turn_correction_mm_s;
    output->left_target_rpm = clamp_wheel_speed(
        speed_mm_s_to_output_rpm(left_speed_mm_s) * APP_H723_LEFT_SIGN);
    output->right_target_rpm = clamp_wheel_speed(
        speed_mm_s_to_output_rpm(right_speed_mm_s) * APP_H723_RIGHT_SIGN);
    output->sequence = input->sequence;
}

void app_line_follow_init(app_line_follow_state_t *state,
                          const PID_Position_Param_Config *params,
                          float dt_s)
{
    if (state == NULL || params == NULL) {
        return;
    }

    PID_Position_Init(&state->pid, params, dt_s);
    state->last_sequence = 0U;
    state->has_sequence = false;
    state->stop_latched = false;
    state->turn_correction_mm_s = 0.0f;
}

void app_line_follow_reset(app_line_follow_state_t *state)
{
    if (state == NULL) {
        return;
    }

    PID_Position_Reset(&state->pid);
    state->last_sequence = 0U;
    state->has_sequence = false;
    state->stop_latched = false;
    state->turn_correction_mm_s = 0.0f;
}

void app_line_follow_step(app_line_follow_state_t *state,
                          const app_line_follow_input_t *input,
                          app_line_follow_output_t *output)
{
    float pid_output;

    if (state == NULL || input == NULL || output == NULL) {
        clear_output(output);
        return;
    }
    if (input->black_count >= APP_H723_LINE_FOLLOW_STOP_BLACK_COUNT) {
        app_line_follow_reset(state);
        state->stop_latched = true;
        clear_output(output);
        return;
    }
    if (state->stop_latched || input->sequence == 0U ||
        input->adc_timeout_mask != 0U ||
        input->line_strength < APP_H723_LINE_FOLLOW_LINE_STRENGTH_MIN) {
        if (!state->stop_latched) {
            app_line_follow_reset(state);
        }
        clear_output(output);
        return;
    }

    if (!state->has_sequence || input->sequence != state->last_sequence) {
        pid_output = PID_Position_Calc(&state->pid, 0.0f, input->line_position);
        state->turn_correction_mm_s = App_Math_ClampFloat(
            pid_output * APP_H723_LINE_FOLLOW_TURN_SIGN,
            -APP_H723_LINE_FOLLOW_MAX_TURN_SPEED_MM_S,
            APP_H723_LINE_FOLLOW_MAX_TURN_SPEED_MM_S);
        state->last_sequence = input->sequence;
        state->has_sequence = true;
    }

    make_output(state, input, output);
}
