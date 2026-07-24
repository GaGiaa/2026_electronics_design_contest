#ifndef BOARD_SERVO_H
#define BOARD_SERVO_H

#include <stdint.h>

void board_servo_init(void);
uint32_t board_servo_set_angle_deg(uint32_t angle_deg);

#endif
