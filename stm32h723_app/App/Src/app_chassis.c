#include "app_chassis.h"

#include <math.h>
#include <string.h>

static float app_chassis_normalize(uint16_t raw)
{
    float value = raw >= 992U ? (float)(raw - 992U) / 819.0f : (float)((int32_t)raw - 992) / 820.0f;
    if (value > 1.0f) { value = 1.0f; }
    if (value < -1.0f) { value = -1.0f; }
    if (fabsf(value) <= APP_H723_CRSF_DEADBAND_NORMALIZED) { return 0.0f; }
    return value > 0.0f ? (value - APP_H723_CRSF_DEADBAND_NORMALIZED) / (1.0f - APP_H723_CRSF_DEADBAND_NORMALIZED) :
                         (value + APP_H723_CRSF_DEADBAND_NORMALIZED) / (1.0f - APP_H723_CRSF_DEADBAND_NORMALIZED);
}

void app_chassis_mix(const app_crsf_input_t *input, uint32_t now_ms, app_chassis_command_t *command)
{
    float left;
    float right;
    float maximum;
    if (command == 0) { return; }
    memset(command, 0, sizeof(*command));
    if (input == 0 || !input->valid || (uint32_t)(now_ms - input->last_valid_ms) >= APP_H723_CRSF_TIMEOUT_MS ||
        input->channels[6] <= 700U || input->channels[6] >= 1300U || input->channels[7] <= 700U || input->channels[7] >= 1300U) { return; }
    command->manual_active = true;
    command->forward_normalized = app_chassis_normalize(input->channels[2]);
    command->turn_normalized = app_chassis_normalize(input->channels[0]);
    left = command->forward_normalized + command->turn_normalized;
    right = command->forward_normalized - command->turn_normalized;
    maximum = fmaxf(fabsf(left), fabsf(right));
    if (maximum > 1.0f) { left /= maximum; right /= maximum; }
    command->left_target_rpm = left * APP_H723_CHASSIS_MAX_RPM * APP_H723_LEFT_SIGN;
    command->right_target_rpm = right * APP_H723_CHASSIS_MAX_RPM * APP_H723_RIGHT_SIGN;
}
