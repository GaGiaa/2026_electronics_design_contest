#include "app_chassis.h"

#include <math.h>
#include <string.h>

#include "app_math.h"
#include "app_task_menu.h"

static float app_chassis_normalize(uint16_t raw)
{
    float value = raw >= 992U ? (float)(raw - 992U) / 819.0f : (float)((int32_t)raw - 992) / 820.0f;
    if (value > 1.0f) { value = 1.0f; }
    if (value < -1.0f) { value = -1.0f; }
    if (fabsf(value) <= APP_H723_CRSF_DEADBAND_NORMALIZED) { return 0.0f; }
    return value > 0.0f ? (value - APP_H723_CRSF_DEADBAND_NORMALIZED) / (1.0f - APP_H723_CRSF_DEADBAND_NORMALIZED) :
                         (value + APP_H723_CRSF_DEADBAND_NORMALIZED) / (1.0f - APP_H723_CRSF_DEADBAND_NORMALIZED);
}

static uint32_t app_chassis_switch_state(uint16_t raw)
{
    if (raw <= 700U) {
        return 0U;
    }
    if (raw >= 1300U) {
        return 2U;
    }
    return 1U;
}

static float app_chassis_line_follow_base_speed(float forward_normalized)
{
    const float speed = forward_normalized * APP_H723_LINE_FOLLOW_STICK_RANGE_MM_S;

    return App_Math_ClampFloat(speed, -APP_H723_LINE_FOLLOW_STICK_RANGE_MM_S,
                               APP_H723_LINE_FOLLOW_STICK_RANGE_MM_S);
}

void app_chassis_mix(const app_crsf_input_t *input, uint32_t now_ms, app_chassis_command_t *command)
{
    float left;
    float right;
    float maximum;
    if (command == 0) { return; }
    memset(command, 0, sizeof(*command));
    command->mode = APP_CHASSIS_MODE_REMOTE_IDLE;
    command->third_motor_target_rpm = 0.0f;
    if (input == 0 || !input->valid ||
        (uint32_t)(now_ms - input->last_valid_ms) >= APP_H723_CRSF_TIMEOUT_MS ||
        input->channels[APP_H723_CRSF_SE_CHANNEL_INDEX] < 1300U ||
        app_chassis_switch_state(input->channels[APP_H723_CRSF_SB_CHANNEL_INDEX]) != 1U) {
        return;
    }
    command->forward_normalized = app_chassis_normalize(input->channels[2]);
    command->base_speed_mm_s = app_chassis_line_follow_base_speed(
        command->forward_normalized);
    command->manual_active = true;
    if (app_chassis_switch_state(input->channels[APP_H723_CRSF_SC_CHANNEL_INDEX]) == 0U) {
        command->mode = APP_CHASSIS_MODE_REMOTE_MANUAL;
    } else if (app_chassis_switch_state(input->channels[APP_H723_CRSF_SC_CHANNEL_INDEX]) == 1U) {
        command->mode = APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW;
        command->left_target_rpm = command->base_speed_mm_s *
                                   APP_H723_MM_S_TO_OUTPUT_RPM * APP_H723_LEFT_SIGN;
        command->right_target_rpm = command->base_speed_mm_s *
                                    APP_H723_MM_S_TO_OUTPUT_RPM * APP_H723_RIGHT_SIGN;
        return;
    } else {
        command->manual_active = false;
        command->mode = APP_CHASSIS_MODE_REMOTE_IDLE;
        command->left_target_rpm = 0.0f;
        command->right_target_rpm = 0.0f;
        return;
    }

    command->turn_normalized = app_chassis_normalize(input->channels[0]);
    left = command->forward_normalized + command->turn_normalized;
    right = command->forward_normalized - command->turn_normalized;
    maximum = fmaxf(fabsf(left), fabsf(right));
    if (maximum > 1.0f) { left /= maximum; right /= maximum; }
    command->left_target_rpm = left * APP_H723_CHASSIS_MAX_OUTPUT_RPM * APP_H723_LEFT_SIGN;
    command->right_target_rpm = right * APP_H723_CHASSIS_MAX_OUTPUT_RPM * APP_H723_RIGHT_SIGN;
}

void app_chassis_control_init(app_chassis_control_state_t *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    app_task_menu_init(NULL);
}

void app_chassis_control_step(app_chassis_control_state_t *state,
                              const app_crsf_input_t *input,
                              uint32_t button_stable_high_mask,
                              uint32_t now_ms,
                              app_chassis_control_output_t *output)
{
    uint32_t rising_buttons;
    bool se_pressed = false;

    if (state == NULL || output == NULL) {
        return;
    }
    memset(output, 0, sizeof(*output));
    output->mode = APP_CHASSIS_MODE_TASK_MENU;
    output->buttons_enabled = true;
    output->chassis.mode = APP_CHASSIS_MODE_TASK_MENU;
    output->chassis.third_motor_target_rpm = 0.0f;
    if (input != NULL) {
        se_pressed = input->channels[APP_H723_CRSF_SE_CHANNEL_INDEX] >= 1300U;
        output->sb_state = app_chassis_switch_state(
            input->channels[APP_H723_CRSF_SB_CHANNEL_INDEX]);
        output->sc_state = app_chassis_switch_state(
            input->channels[APP_H723_CRSF_SC_CHANNEL_INDEX]);
    }
    output->se_pressed = se_pressed;

    if (!state->initialized) {
        state->initialized = true;
        state->last_button_mask = button_stable_high_mask;
        state->previous_se_pressed = se_pressed;
    } else if (state->previous_se_pressed && !se_pressed) {
        app_task_menu_reset();
        state->last_button_mask = button_stable_high_mask;
    }

    if (!se_pressed) {
        rising_buttons = button_stable_high_mask & ~state->last_button_mask;
        if ((rising_buttons & (1U << 0U)) != 0U) {
            app_task_menu_key_event(APP_TASK_MENU_KEY_CONFIRM, now_ms);
        }
        if ((rising_buttons & (1U << 1U)) != 0U) {
            app_task_menu_key_event(APP_TASK_MENU_KEY_PREV, now_ms);
        }
        if ((rising_buttons & (1U << 2U)) != 0U) {
            app_task_menu_key_event(APP_TASK_MENU_KEY_NEXT, now_ms);
        }
    } else {
        output->remote_takeover = true;
        output->buttons_enabled = false;
        if (input != NULL && input->valid &&
            (uint32_t)(now_ms - input->last_valid_ms) < APP_H723_CRSF_TIMEOUT_MS) {
            app_chassis_mix(input, now_ms, &output->chassis);
            output->mode = output->chassis.mode;
        } else {
            output->mode = APP_CHASSIS_MODE_REMOTE_IDLE;
            output->chassis.mode = APP_CHASSIS_MODE_REMOTE_IDLE;
        }
    }

    state->last_button_mask = button_stable_high_mask;
    state->previous_se_pressed = se_pressed;
    output->selected_task = app_task_menu_selected_task();
    output->task_request_available = app_task_menu_execution_requested();
    if (!se_pressed) {
        output->mode = APP_CHASSIS_MODE_TASK_MENU;
        output->chassis.mode = APP_CHASSIS_MODE_TASK_MENU;
        output->chassis.manual_active = false;
        output->chassis.left_target_rpm = 0.0f;
        output->chassis.right_target_rpm = 0.0f;
        output->chassis.third_motor_target_rpm = 0.0f;
    }
}
