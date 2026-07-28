#ifndef ULTRASONIC_MEASUREMENT_H
#define ULTRASONIC_MEASUREMENT_H

#include <stdbool.h>
#include <stdint.h>

bool ultrasonic_measurement_calculate(uint32_t ticks, uint32_t timer_hz,
                                      uint32_t min_echo_us,
                                      uint32_t max_echo_us,
                                      uint32_t *echo_us,
                                      uint32_t *distance_mm);

#endif
