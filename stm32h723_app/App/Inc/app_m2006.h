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

/**
 * @brief M2006 转子编码器的连续多圈位置跟踪状态。
 *
 * 首次样本只建立编码器基准，不产生位移；后续样本按 8192 counts/转展开。
 * `motor_counts` 相对于首次样本，输出轴角度还会除以 `APP_H723_M2006_GEAR_RATIO`。
 */
typedef struct {
    bool initialized;
    uint16_t last_encoder;
    int64_t motor_counts;
} app_m2006_position_tracker_t;

bool app_m2006_parse_feedback(uint32_t standard_id, const uint8_t data[8], app_m2006_feedback_t *feedback);
float app_m2006_raw_current_to_a(int16_t raw_current);
int16_t app_m2006_current_a_to_raw(float current_a);
void app_m2006_encode_group_current_slots(const int16_t currents[3], uint8_t data[8]);
void app_m2006_encode_group_current(int16_t left_current, int16_t right_current, uint8_t data[8]);
void app_m2006_position_tracker_init(app_m2006_position_tracker_t *tracker);
bool app_m2006_position_tracker_update(app_m2006_position_tracker_t *tracker, uint16_t encoder);
int64_t app_m2006_position_tracker_motor_counts(const app_m2006_position_tracker_t *tracker);
float app_m2006_position_tracker_output_degrees(const app_m2006_position_tracker_t *tracker);

#endif
