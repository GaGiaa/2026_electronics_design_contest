#include "algorithms/ultrasonic/ultrasonic_measurement.h"

#include <limits.h>
#include <stddef.h>

bool ultrasonic_measurement_calculate(uint32_t ticks, uint32_t timer_hz,
                                      uint32_t min_echo_us,
                                      uint32_t max_echo_us,
                                      uint32_t *echo_us,
                                      uint32_t *distance_mm)
{
    uint64_t echo_us_rounded;
    uint64_t distance_mm_rounded;

    if ((timer_hz == 0U) || (min_echo_us > max_echo_us) ||
        (echo_us == NULL) || (distance_mm == NULL)) {
        return false;
    }

    echo_us_rounded = (((uint64_t)ticks * 1000000ULL) +
                       ((uint64_t)timer_hz / 2ULL)) /
                      (uint64_t)timer_hz;
    if ((echo_us_rounded > UINT32_MAX) ||
        (echo_us_rounded < (uint64_t)min_echo_us) ||
        (echo_us_rounded > (uint64_t)max_echo_us)) {
        return false;
    }

    distance_mm_rounded = (echo_us_rounded * 343ULL + 1000ULL) / 2000ULL;
    if (distance_mm_rounded > UINT32_MAX) {
        return false;
    }

    *echo_us = (uint32_t)echo_us_rounded;
    *distance_mm = (uint32_t)distance_mm_rounded;
    return true;
}
