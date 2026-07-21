#ifndef BOARD_MOTOR_H
#define BOARD_MOTOR_H

#include <stdint.h>

typedef enum {
    BOARD_MOTOR_FRONT_LEFT = 0U,
    BOARD_MOTOR_FRONT_RIGHT,
    BOARD_MOTOR_REAR_LEFT,
    BOARD_MOTOR_REAR_RIGHT,
    BOARD_MOTOR_COUNT
} board_motor_wheel_t;

typedef enum {
    BOARD_MOTOR_DIRECTION_STOP = 0U,
    BOARD_MOTOR_DIRECTION_FORWARD,
    BOARD_MOTOR_DIRECTION_REVERSE
} board_motor_direction_t;

void board_motor_set(board_motor_wheel_t wheel, board_motor_direction_t direction,
                     uint8_t duty_percent);

#endif
