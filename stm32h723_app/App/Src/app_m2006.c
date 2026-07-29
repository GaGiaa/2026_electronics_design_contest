#include "app_m2006.h"
#include "app_config.h"

#include <math.h>
#include <stddef.h>

#define APP_M2006_ENCODER_COUNTS_PER_REVOLUTION 8192

float app_m2006_raw_current_to_a(int16_t raw_current)
{
    return ((float)raw_current * APP_H723_M2006_C610_MAX_CURRENT_A) /
           (float)APP_H723_M2006_CURRENT_MAX_RAW;
}

int16_t app_m2006_current_a_to_raw(float current_a)
{
    float limited_current_a;
    float raw_current;

    if (!isfinite(current_a)) {
        return 0;
    }
    limited_current_a = current_a;
    if (limited_current_a > APP_H723_M2006_C610_MAX_CURRENT_A) {
        limited_current_a = APP_H723_M2006_C610_MAX_CURRENT_A;
    } else if (limited_current_a < -APP_H723_M2006_C610_MAX_CURRENT_A) {
        limited_current_a = -APP_H723_M2006_C610_MAX_CURRENT_A;
    }
    raw_current = limited_current_a * (float)APP_H723_M2006_CURRENT_MAX_RAW /
                  APP_H723_M2006_C610_MAX_CURRENT_A;
    return (int16_t)(raw_current >= 0.0f ? raw_current + 0.5f : raw_current - 0.5f);
}

bool app_m2006_parse_feedback(uint32_t standard_id, const uint8_t data[8], app_m2006_feedback_t *feedback)
{
    if (data == 0 || feedback == 0 || standard_id < 0x201U || standard_id > 0x203U) { return false; }
    feedback->motor_id = (uint8_t)(standard_id - 0x200U);
    feedback->encoder = (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
    feedback->rotor_speed_rpm = (int16_t)(((uint16_t)data[2] << 8U) | data[3]);
    feedback->current_raw = (int16_t)(((uint16_t)data[4] << 8U) | data[5]);
    feedback->output_speed_rpm = (float)feedback->rotor_speed_rpm / APP_H723_M2006_GEAR_RATIO;
    feedback->current_a = app_m2006_raw_current_to_a(feedback->current_raw);
    feedback->temperature_celsius = data[6];
    return true;
}

void app_m2006_encode_group_current_slots(const int16_t currents[3], uint8_t data[8])
{
    uint32_t index;
    if (currents == 0 || data == 0) { return; }
    for (index = 0U; index < 3U; ++index) {
        data[index * 2U] = (uint8_t)((uint16_t)currents[index] >> 8U);
        data[index * 2U + 1U] = (uint8_t)currents[index];
    }
    data[6] = 0U; data[7] = 0U;
}

void app_m2006_encode_group_current(int16_t left_current, int16_t right_current, uint8_t data[8])
{
    const int16_t currents[3] = {left_current, right_current, 0};
    app_m2006_encode_group_current_slots(currents, data);
}

void app_m2006_position_tracker_init(app_m2006_position_tracker_t *tracker)
{
    if (tracker == NULL) {
        return;
    }
    tracker->initialized = false;
    tracker->last_encoder = 0U;
    tracker->motor_counts = 0;
}

bool app_m2006_position_tracker_update(app_m2006_position_tracker_t *tracker, uint16_t encoder)
{
    int32_t delta;

    if (tracker == NULL || encoder >= APP_M2006_ENCODER_COUNTS_PER_REVOLUTION) {
        return false;
    }
    if (!tracker->initialized) {
        tracker->initialized = true;
        tracker->last_encoder = encoder;
        tracker->motor_counts = 0;
        return false;
    }

    delta = (int32_t)encoder - (int32_t)tracker->last_encoder;
    if (delta > (APP_M2006_ENCODER_COUNTS_PER_REVOLUTION / 2)) {
        delta -= APP_M2006_ENCODER_COUNTS_PER_REVOLUTION;
    } else if (delta < -(APP_M2006_ENCODER_COUNTS_PER_REVOLUTION / 2)) {
        delta += APP_M2006_ENCODER_COUNTS_PER_REVOLUTION;
    }
    tracker->motor_counts += delta;
    tracker->last_encoder = encoder;
    return true;
}

int64_t app_m2006_position_tracker_motor_counts(const app_m2006_position_tracker_t *tracker)
{
    return tracker != NULL ? tracker->motor_counts : 0;
}

float app_m2006_position_tracker_output_degrees(const app_m2006_position_tracker_t *tracker)
{
    if (tracker == NULL) {
        return 0.0f;
    }
    return ((float)tracker->motor_counts * 360.0f) /
           ((float)APP_M2006_ENCODER_COUNTS_PER_REVOLUTION * APP_H723_M2006_GEAR_RATIO);
}
