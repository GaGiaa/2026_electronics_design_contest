#include "crsf_control.h"

#include <math.h>
#include <stddef.h>

static float clamp_unit(float value)
{
    if (value < -1.0f) {
        return -1.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static float normalize_channel(uint16_t raw)
{
    float normalized;
    float deadband = CRSF_CHANNEL_DEADBAND;

    if (raw >= CRSF_CHANNEL_CENTER) {
        normalized = (float)(raw - CRSF_CHANNEL_CENTER) /
                     (float)(CRSF_CHANNEL_MAX - CRSF_CHANNEL_CENTER);
    } else {
        normalized = (float)((int32_t)raw - (int32_t)CRSF_CHANNEL_CENTER) /
                     (float)(CRSF_CHANNEL_CENTER - CRSF_CHANNEL_MIN);
    }
    normalized = clamp_unit(normalized);
    if (fabsf(normalized) <= deadband) {
        return 0.0f;
    }
    if (normalized > 0.0f) {
        return clamp_unit((normalized - deadband) / (1.0f - deadband));
    }
    return clamp_unit((normalized + deadband) / (1.0f - deadband));
}

static void clear_targets(float targets[BOARD_MOTOR_COUNT])
{
    size_t wheel;

    for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
        targets[wheel] = 0.0f;
    }
}

static bool input_link_is_active(const crsf_control_input_t *input,
                                 uint32_t now_ms)
{
    return input != NULL && input->valid &&
           (uint32_t)(now_ms - input->last_valid_time_ms) <
               CRSF_LINK_TIMEOUT_MS;
}

crsf_drive_mode_t crsf_control_get_drive_mode(
    const crsf_control_input_t *input, uint32_t now_ms)
{
    uint16_t mode_value;

    if (!input_link_is_active(input, now_ms)) {
        return CRSF_DRIVE_MODE_IDLE;
    }

    mode_value = input->channels[CRSF_MODE_CHANNEL_INDEX];
    if ((mode_value < CRSF_CHANNEL_MIN) || (mode_value > CRSF_CHANNEL_MAX)) {
        return CRSF_DRIVE_MODE_IDLE;
    }
    if (mode_value <= CRSF_MODE_LOW_MAX) {
        return CRSF_DRIVE_MODE_IDLE;
    }
    if (mode_value >= CRSF_MODE_HIGH_MIN) {
        return CRSF_DRIVE_MODE_LINE_TRACKING;
    }
    return CRSF_DRIVE_MODE_MANUAL;
}

bool crsf_control_get_forward_speed(const crsf_control_input_t *input,
                                    uint32_t now_ms,
                                    float *speed_mm_per_s)
{
    if (speed_mm_per_s == NULL) {
        return false;
    }
    *speed_mm_per_s = 0.0f;
    if (!input_link_is_active(input, now_ms)) {
        return false;
    }

    *speed_mm_per_s = normalize_channel(
                          input->channels[CRSF_FORWARD_CHANNEL_INDEX]) *
                      CRSF_FORWARD_SIGN * CRSF_MAX_SPEED_MM_PER_S;
    return true;
}

bool crsf_control_mix(const crsf_control_input_t *input,
                      uint32_t now_ms,
                      float targets[BOARD_MOTOR_COUNT])
{
    float forward;
    float turn;
    float left;
    float right;
    float maximum;

    if (targets == NULL) {
        return false;
    }
    clear_targets(targets);
    if (!input_link_is_active(input, now_ms)) {
        return false;
    }

    forward = normalize_channel(input->channels[CRSF_FORWARD_CHANNEL_INDEX]) * CRSF_FORWARD_SIGN;
    turn = normalize_channel(input->channels[CRSF_TURN_CHANNEL_INDEX]) * CRSF_TURN_SIGN;
    left = forward + turn;
    right = forward - turn;
    maximum = fmaxf(fabsf(left), fabsf(right));
    if (maximum > 1.0f) {
        left /= maximum;
        right /= maximum;
    }

    targets[BOARD_MOTOR_FRONT_LEFT] = left * CRSF_MAX_SPEED_MM_PER_S;
    targets[BOARD_MOTOR_REAR_LEFT] = left * CRSF_MAX_SPEED_MM_PER_S;
    targets[BOARD_MOTOR_FRONT_RIGHT] = right * CRSF_MAX_SPEED_MM_PER_S;
    targets[BOARD_MOTOR_REAR_RIGHT] = right * CRSF_MAX_SPEED_MM_PER_S;
    return true;
}
