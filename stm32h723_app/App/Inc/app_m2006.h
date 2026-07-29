#ifndef APP_M2006_H
#define APP_M2006_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t motor_id;
    uint16_t encoder;
    int16_t speed_rpm;
    int16_t current;
    uint8_t temperature_celsius;
} app_m2006_feedback_t;

bool app_m2006_parse_feedback(uint32_t standard_id, const uint8_t data[8], app_m2006_feedback_t *feedback);
void app_m2006_encode_group_current_slots(const int16_t currents[3], uint8_t data[8]);
void app_m2006_encode_group_current(int16_t left_current, int16_t right_current, uint8_t data[8]);

#endif
