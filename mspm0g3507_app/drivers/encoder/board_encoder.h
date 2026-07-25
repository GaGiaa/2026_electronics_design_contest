#ifndef BOARD_ENCODER_H
#define BOARD_ENCODER_H

#include <stdint.h>

#include "config/encoder_config.h"
#include "drivers/motor/board_motor.h"

typedef struct {
    int32_t delta_counts;
    int32_t total_counts;
    float instant_speed_mm_per_s;
    float speed_mm_per_s;
} board_encoder_sample_t;

void board_encoder_init(void);
void board_encoder_gpioa_irq_handler(void);
void board_encoder_gpiob_irq_handler(void);
board_encoder_sample_t board_encoder_sample(board_motor_wheel_t wheel);

#endif
