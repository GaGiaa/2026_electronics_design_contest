#include "app_chassis.h"

#include <math.h>
#include <string.h>

#include "app_math.h"

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
    const float speed = APP_H723_LINE_FOLLOW_BASE_SPEED_MM_S +
                        forward_normalized * APP_H723_LINE_FOLLOW_STICK_RANGE_MM_S;

    return App_Math_ClampFloat(speed, 0.0f, APP_H723_LINE_FOLLOW_MAX_SPEED_MM_S);
}

void app_chassis_mix(const app_crsf_input_t *input, uint32_t now_ms, app_chassis_command_t *command)
{
    float left;
    float right;
    float maximum;
    if (command == 0) { return; }
    memset(command, 0, sizeof(*command));
    command->mode = APP_CHASSIS_MODE_STOP;
    if (input == 0 || !input->valid ||
        (uint32_t)(now_ms - input->last_valid_ms) >= APP_H723_CRSF_TIMEOUT_MS ||
        app_chassis_switch_state(input->channels[7]) != 1U ||
        app_chassis_switch_state(input->channels[6]) == 0U) {
        return;
    }
    command->forward_normalized = app_chassis_normalize(input->channels[2]);
    command->base_speed_mm_s = app_chassis_line_follow_base_speed(
        command->forward_normalized);
    command->manual_active = true;
    if (app_chassis_switch_state(input->channels[6]) == 2U) {
        command->mode = APP_CHASSIS_MODE_LINE_FOLLOW;
        command->left_target_rpm = command->base_speed_mm_s *
                                   APP_H723_MM_S_TO_OUTPUT_RPM * APP_H723_LEFT_SIGN;
        command->right_target_rpm = command->base_speed_mm_s *
                                    APP_H723_MM_S_TO_OUTPUT_RPM * APP_H723_RIGHT_SIGN;
        return;
    }

    command->mode = APP_CHASSIS_MODE_MANUAL;
    command->turn_normalized = app_chassis_normalize(input->channels[0]);
    left = command->forward_normalized + command->turn_normalized;
    right = command->forward_normalized - command->turn_normalized;
    maximum = fmaxf(fabsf(left), fabsf(right));
    if (maximum > 1.0f) { left /= maximum; right /= maximum; }
    command->left_target_rpm = left * APP_H723_CHASSIS_MAX_OUTPUT_RPM * APP_H723_LEFT_SIGN;
    command->right_target_rpm = right * APP_H723_CHASSIS_MAX_OUTPUT_RPM * APP_H723_RIGHT_SIGN;
}
