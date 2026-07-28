#ifndef COURSE_FOLLOWING_TELEMETRY_H
#define COURSE_FOLLOWING_TELEMETRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "drivers/motor/board_motor.h"
#include "protocols/vofa/vofa_justfloat.h"

#define COURSE_FOLLOWING_TELEMETRY_CHANNEL_COUNT 14U

typedef struct {
    float yaw_deg;
    float yaw_rate_dps;
    float gyro_bias_z_dps;
    float heading_target_deg;
    bool heading_hold;
    int32_t line_error;
    float target_speed_mm_per_s[BOARD_MOTOR_COUNT];
    float feedback_speed_mm_per_s[BOARD_MOTOR_COUNT];
} course_following_telemetry_values_t;

bool course_following_telemetry_encode(
    uint8_t *frame, size_t frame_size,
    const course_following_telemetry_values_t *values);

#endif
