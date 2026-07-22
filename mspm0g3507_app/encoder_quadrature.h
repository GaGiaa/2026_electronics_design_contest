#ifndef ENCODER_QUADRATURE_H
#define ENCODER_QUADRATURE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t previous_state;
} board_encoder_quadrature_t;

void board_encoder_quadrature_init(board_encoder_quadrature_t *decoder,
                                   bool a_is_high, bool b_is_high);
int32_t board_encoder_quadrature_update(board_encoder_quadrature_t *decoder,
                                         bool a_is_high, bool b_is_high);

#endif
