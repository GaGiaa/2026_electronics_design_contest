#ifndef APP_M2006_H
#define APP_M2006_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t motor_id;
    uint16_t encoder;
    int16_t rotor_speed_rpm;
    int16_t current_raw;
    float output_speed_rpm;
    float current_a;
    uint8_t temperature_celsius;
} app_m2006_feedback_t;

bool app_m2006_parse_feedback(uint32_t standard_id, const uint8_t data[8], app_m2006_feedback_t *feedback);
float app_m2006_raw_current_to_a(int16_t raw_current);
int16_t app_m2006_current_a_to_raw(float current_a);
void app_m2006_encode_group_current_slots(const int16_t currents[3], uint8_t data[8]);
void app_m2006_encode_group_current(int16_t left_current, int16_t right_current, uint8_t data[8]);

#endif
