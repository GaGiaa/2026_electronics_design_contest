#include "encoder_quadrature.h"

static uint8_t encoder_state(bool a_is_high, bool b_is_high)
{
    return (uint8_t)(((a_is_high ? 1U : 0U) << 1U) |
                     (b_is_high ? 1U : 0U));
}

void board_encoder_quadrature_init(board_encoder_quadrature_t *decoder,
                                   bool a_is_high, bool b_is_high)
{
    decoder->previous_state = encoder_state(a_is_high, b_is_high);
}

int32_t board_encoder_quadrature_update(board_encoder_quadrature_t *decoder,
                                         bool a_is_high, bool b_is_high)
{
    static const int8_t transitions[16] = {
        0, -1, 1, 0,
        1, 0, 0, -1,
        -1, 0, 0, 1,
        0, 1, -1, 0,
    };
    uint8_t current_state = encoder_state(a_is_high, b_is_high);
    uint8_t transition = (uint8_t)((decoder->previous_state << 2U) | current_state);
    int32_t delta = transitions[transition];

    decoder->previous_state = current_state;
    return delta;
}
