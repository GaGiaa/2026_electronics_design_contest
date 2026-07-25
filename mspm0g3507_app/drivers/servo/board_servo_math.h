#ifndef BOARD_SERVO_MATH_H
#define BOARD_SERVO_MATH_H

#include <stdint.h>

#include "config/app_config.h"

static inline uint32_t board_servo_clamp_angle_deg(uint32_t angle_deg)
{
    return (angle_deg > SERVO_MAX_ANGLE_DEG) ? SERVO_MAX_ANGLE_DEG : angle_deg;
}

static inline uint32_t board_servo_angle_to_pulse_us(uint32_t angle_deg)
{
    const uint32_t clamped_angle_deg = board_servo_clamp_angle_deg(angle_deg);
    const uint64_t pulse_span_us =
        (uint64_t)SERVO_MAX_PULSE_US - (uint64_t)SERVO_MIN_PULSE_US;

    return SERVO_MIN_PULSE_US +
           (uint32_t)((pulse_span_us * clamped_angle_deg) /
                      SERVO_MAX_ANGLE_DEG);
}

#endif
