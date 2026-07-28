#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "protocols/vofa/course_following_telemetry.h"

static float read_float(const uint8_t *frame, uint32_t channel)
{
    float value;

    memcpy(&value, &frame[channel * VOFA_JUSTFLOAT_CHANNEL_SIZE],
           sizeof(value));
    return value;
}

int main(void)
{
    course_following_telemetry_values_t values = {0};
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(
        COURSE_FOLLOWING_TELEMETRY_CHANNEL_COUNT)];
    uint32_t wheel;

    values.yaw_deg = 12.5f;
    values.yaw_rate_dps = -3.25f;
    values.gyro_bias_z_dps = 0.18f;
    values.heading_target_deg = 175.0f;
    values.heading_hold = true;
    values.line_error = -1234;
    for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
        values.target_speed_mm_per_s[wheel] = 100.0f + (float)wheel;
        values.feedback_speed_mm_per_s[wheel] = 80.0f + (float)wheel;
    }

    assert(course_following_telemetry_encode(frame, sizeof(frame), &values));
    assert(fabsf(read_float(frame, 0U) - 12.5f) < 0.001f);
    assert(fabsf(read_float(frame, 1U) + 3.25f) < 0.001f);
    assert(fabsf(read_float(frame, 2U) - 0.18f) < 0.001f);
    assert(fabsf(read_float(frame, 3U) - 175.0f) < 0.001f);
    assert(fabsf(read_float(frame, 4U) - 1.0f) < 0.001f);
    assert(fabsf(read_float(frame, 5U) + 1234.0f) < 0.001f);
    assert(fabsf(read_float(frame, 6U) - 100.0f) < 0.001f);
    assert(fabsf(read_float(frame, 9U) - 103.0f) < 0.001f);
    assert(fabsf(read_float(frame, 10U) - 80.0f) < 0.001f);
    assert(fabsf(read_float(frame, 13U) - 83.0f) < 0.001f);
    assert(frame[56U] == 0x00U);
    assert(frame[57U] == 0x00U);
    assert(frame[58U] == 0x80U);
    assert(frame[59U] == 0x7FU);
    return 0;
}
