#ifndef BOARD_WS2812_H
#define BOARD_WS2812_H

#include <stdint.h>

#define BOARD_WS2812_PIXEL_COUNT 4U

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} board_ws2812_pixel_t;

void board_ws2812_write(const board_ws2812_pixel_t pixels[BOARD_WS2812_PIXEL_COUNT]);

#endif
