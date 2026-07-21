#ifndef BOARD_ENCODER_H
#define BOARD_ENCODER_H

#include <stdint.h>

#include "board_motor.h"

#define BOARD_ENCODER_COUNTS_PER_REVOLUTION 1040
#define BOARD_ENCODER_WHEEL_DIAMETER_MM 48
#define BOARD_ENCODER_SAMPLE_PERIOD_MS 10U

typedef struct {
    int32_t delta_counts;
    int32_t total_counts;
    float speed_mm_per_s;
} board_encoder_sample_t;

void board_encoder_init(void);
void board_encoder_gpioa_irq_handler(void);
board_encoder_sample_t board_encoder_sample(board_motor_wheel_t wheel);

#endif
