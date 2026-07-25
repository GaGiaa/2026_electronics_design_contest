#include <stdint.h>
#include <stdio.h>

#include "drivers/servo/board_servo_math.h"

static int check_equal(const char *name, uint32_t actual, uint32_t expected)
{
    if (actual != expected) {
        fprintf(stderr, "%s: expected %lu, got %lu\n",
                name,
                (unsigned long)expected,
                (unsigned long)actual);
        return 0;
    }
    return 1;
}

int main(void)
{
    int passed = 1;

    passed &= check_equal("angle 0", board_servo_angle_to_pulse_us(0U),
                         APP_SERVO_MIN_PULSE_US);
    passed &= check_equal("angle midpoint",
                         board_servo_angle_to_pulse_us(APP_SERVO_MAX_ANGLE_DEG / 2U),
                         (APP_SERVO_MIN_PULSE_US + APP_SERVO_MAX_PULSE_US) / 2U);
    passed &= check_equal("angle max",
                         board_servo_angle_to_pulse_us(APP_SERVO_MAX_ANGLE_DEG),
                         APP_SERVO_MAX_PULSE_US);
    passed &= check_equal("angle above max",
                         board_servo_angle_to_pulse_us(APP_SERVO_MAX_ANGLE_DEG + 100U),
                         APP_SERVO_MAX_PULSE_US);

    return passed ? 0 : 1;
}
