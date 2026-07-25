#include "board_ws2812.h"

#include <FreeRTOS.h>
#include <task.h>

#include "ti_msp_dl_config.h"

#define WS2812_SPI_SYMBOL_BITS          3U
#define WS2812_SPI_ZERO_SYMBOL        0x04U
#define WS2812_SPI_ONE_SYMBOL         0x06U
#define WS2812_SPI_BYTES_PER_COLOR     3U
#define WS2812_SPI_DATA_BYTES         36U
#define WS2812_SPI_RESET_BYTES        24U
#define WS2812_SPI_FRAME_BYTES        60U

static uint8_t g_spi_frame[WS2812_SPI_FRAME_BYTES];

static void send_byte(uint8_t value, uint8_t *encoded)
{
    uint32_t symbols = 0U;
    uint8_t mask;

    for (mask = 0x80U; mask != 0U; mask >>= 1U) {
        symbols <<= WS2812_SPI_SYMBOL_BITS;
        symbols |= ((value & mask) != 0U) ? WS2812_SPI_ONE_SYMBOL : WS2812_SPI_ZERO_SYMBOL;
    }

    encoded[0] = (uint8_t)(symbols >> 16U);
    encoded[1] = (uint8_t)(symbols >> 8U);
    encoded[2] = (uint8_t)symbols;
}

void board_ws2812_write(const board_ws2812_pixel_t pixels[BOARD_WS2812_PIXEL_COUNT])
{
    uint32_t index;
    uint32_t frame_index = 0U;

    taskDISABLE_INTERRUPTS();
    for (index = 0U; index < BOARD_WS2812_PIXEL_COUNT; ++index) {
        const board_ws2812_pixel_t *pixel = &pixels[index];
        send_byte(pixel->green, &g_spi_frame[frame_index]);
        frame_index += WS2812_SPI_BYTES_PER_COLOR;
        send_byte(pixel->red, &g_spi_frame[frame_index]);
        frame_index += WS2812_SPI_BYTES_PER_COLOR;
        send_byte(pixel->blue, &g_spi_frame[frame_index]);
        frame_index += WS2812_SPI_BYTES_PER_COLOR;
    }
    while (frame_index < WS2812_SPI_FRAME_BYTES) {
        g_spi_frame[frame_index++] = 0U;
    }
    for (frame_index = 0U; frame_index < WS2812_SPI_FRAME_BYTES; ++frame_index) {
        while (DL_SPI_isTXFIFOFull(SPI_WS2812_INST)) {
        }
        DL_SPI_transmitData8(SPI_WS2812_INST, g_spi_frame[frame_index]);
    }
    while (DL_SPI_isBusy(SPI_WS2812_INST)) {
    }
    taskENABLE_INTERRUPTS();
}
