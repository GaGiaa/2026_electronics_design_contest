#ifndef BOARD_GRAYSCALE_H
#define BOARD_GRAYSCALE_H

#include <stdint.h>

#define BOARD_GRAYSCALE_CHANNEL_COUNT 8U
#define BOARD_GRAYSCALE_ADC_MAX 4095U

typedef struct {
    uint16_t raw[BOARD_GRAYSCALE_CHANNEL_COUNT];
    uint16_t normalized[BOARD_GRAYSCALE_CHANNEL_COUNT];
    /* Existing hysteresis result: 1 means white, 0 means black. */
    uint8_t digital;
    /* Derived line observation: bit N corresponds to channel N; 1 means black. */
    uint8_t black_mask;
    /* ADC timeout observation: bit N means channel N timed out in this sample. */
    uint8_t adc_timeout_mask;
    uint8_t black_count;
    uint32_t line_strength;
    int32_t line_error;
    uint32_t sequence;
} board_grayscale_snapshot_t;

extern volatile uint32_t g_grayscale_adc_timeout_count;

void board_grayscale_init(const uint16_t *white, const uint16_t *black);
void board_grayscale_sample(board_grayscale_snapshot_t *snapshot);

#endif
