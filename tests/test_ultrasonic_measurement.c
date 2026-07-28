#include <stdint.h>
#include <stdio.h>

#include "algorithms/ultrasonic/ultrasonic_measurement.h"

static int expect_measurement(const char *name, uint32_t ticks,
                              uint32_t timer_hz, uint32_t min_echo_us,
                              uint32_t max_echo_us, uint32_t expected_echo_us,
                              uint32_t expected_distance_mm)
{
    uint32_t echo_us = 0U;
    uint32_t distance_mm = 0U;

    if (!ultrasonic_measurement_calculate(
            ticks, timer_hz, min_echo_us, max_echo_us, &echo_us,
            &distance_mm)) {
        fprintf(stderr, "%s: expected a valid measurement\n", name);
        return 0;
    }
    if ((echo_us != expected_echo_us) ||
        (distance_mm != expected_distance_mm)) {
        fprintf(stderr,
                "%s: expected %lu us/%lu mm, got %lu us/%lu mm\n", name,
                (unsigned long)expected_echo_us,
                (unsigned long)expected_distance_mm, (unsigned long)echo_us,
                (unsigned long)distance_mm);
        return 0;
    }
    return 1;
}

static int expect_invalid(const char *name, uint32_t ticks, uint32_t timer_hz,
                          uint32_t min_echo_us, uint32_t max_echo_us,
                          uint32_t *echo_us, uint32_t *distance_mm)
{
    if (ultrasonic_measurement_calculate(ticks, timer_hz, min_echo_us,
                                         max_echo_us, echo_us, distance_mm)) {
        fprintf(stderr, "%s: expected an invalid measurement\n", name);
        return 0;
    }
    return 1;
}

int main(void)
{
    int passed = 1;
    uint32_t echo_us = 0U;
    uint32_t distance_mm = 0U;

    passed &= expect_measurement("1 MHz conversion", 10000U, 1000000U,
                                 100U, 30000U, 10000U, 1715U);
    passed &= expect_measurement("10 MHz conversion", 12345U, 10000000U,
                                 100U, 30000U, 1235U, 212U);
    passed &= expect_measurement("minimum boundary", 100U, 1000000U, 100U,
                                 30000U, 100U, 17U);
    passed &= expect_measurement("maximum boundary", 30000U, 1000000U, 100U,
                                 30000U, 30000U, 5145U);
    passed &= expect_measurement("distance rounding", 1001U, 1000000U, 100U,
                                 30000U, 1001U, 172U);

    passed &= expect_invalid("below minimum", 99U, 1000000U, 100U, 30000U,
                             &echo_us, &distance_mm);
    passed &= expect_invalid("above maximum", 30001U, 1000000U, 100U,
                             30000U, &echo_us, &distance_mm);
    passed &= expect_invalid("zero timer frequency", 10000U, 0U, 100U,
                             30000U, &echo_us, &distance_mm);
    passed &= expect_invalid("null echo output", 10000U, 1000000U, 100U,
                             30000U, NULL, &distance_mm);
    passed &= expect_invalid("null distance output", 10000U, 1000000U, 100U,
                             30000U, &echo_us, NULL);

    return passed ? 0 : 1;
}
