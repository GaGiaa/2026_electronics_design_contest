#include "course_following_telemetry.h"

#include <stddef.h>

bool course_following_telemetry_encode(
    uint8_t *frame, size_t frame_size,
    const course_following_telemetry_values_t *values)
{
    float channels[COURSE_FOLLOWING_TELEMETRY_CHANNEL_COUNT];
    uint32_t wheel;

    if ((frame == NULL) || (values == NULL)) {
        return false;
    }
    channels[0U] = values->yaw_deg;
    channels[1U] = values->yaw_rate_dps;
    channels[2U] = values->gyro_bias_z_dps;
    channels[3U] = values->heading_target_deg;
    channels[4U] = values->heading_hold ? 1.0f : 0.0f;
    channels[5U] = (float)values->line_error;
    for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
        channels[6U + wheel] = values->target_speed_mm_per_s[wheel];
        channels[10U + wheel] = values->feedback_speed_mm_per_s[wheel];
    }
    return vofa_justfloat_encode(frame, frame_size, channels,
                                 COURSE_FOLLOWING_TELEMETRY_CHANNEL_COUNT);
}
