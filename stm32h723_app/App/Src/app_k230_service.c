#include "app_k230_service.h"

#include <limits.h>

#include "app_config.h"
#include "app_debug.h"
#include "app_k230.h"
#include "usart.h"

#if (APP_H723_K230_UART2_TEST_ENABLE == 1U)

#define H723_K230_DMA_BUFFER_SIZE 64U
#define H723_K230_RING_SIZE 256U

static uint8_t s_dma_buffer[H723_K230_DMA_BUFFER_SIZE];
static volatile uint8_t s_ring[H723_K230_RING_SIZE];
static volatile uint16_t s_write_index;
static uint16_t s_read_index;
static uint32_t s_uart_error_count;
static uint32_t s_ring_overrun_count;
static app_k230_parser_t s_parser;
static app_k230_sample_t s_sample;

static void h723_k230_start_receive(void)
{
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart2, s_dma_buffer, sizeof(s_dma_buffer)) == HAL_OK &&
        huart2.hdmarx != NULL) {
        __HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT);
    }
}

static void h723_k230_publish_debug(uint32_t now_ms)
{
    g_h723_debug.ball_vision.distance_mm = s_sample.distance_mm;
    g_h723_debug.ball_vision.valid = s_sample.valid ? 1U : 0U;
    g_h723_debug.ball_vision.valid_frame_count = s_sample.valid_frame_count;
    g_h723_debug.ball_vision.crc_error_count = s_sample.crc_error_count;
    g_h723_debug.ball_vision.format_error_count = s_sample.format_error_count;
    g_h723_debug.ball_vision.uart_error_count = s_uart_error_count;
    g_h723_debug.ball_vision.ring_overrun_count = s_ring_overrun_count;
    g_h723_debug.ball_vision.frame_age_ms = s_sample.valid_frame_count != 0U ?
        (uint32_t)(now_ms - s_sample.last_frame_ms) : UINT_MAX;
    g_h723_debug.ball_vision.dma_active = huart2.RxState == HAL_UART_STATE_BUSY_RX ? 1U : 0U;
}

void h723_k230_service_init(void)
{
    app_k230_parser_init(&s_parser);
    h723_k230_start_receive();
}

void h723_k230_on_uart2_rx_event(uint16_t size)
{
    uint16_t index;

    if (size > H723_K230_DMA_BUFFER_SIZE) {
        size = H723_K230_DMA_BUFFER_SIZE;
    }
    for (index = 0U; index < size; ++index) {
        uint16_t next = (uint16_t)((s_write_index + 1U) % H723_K230_RING_SIZE);
        if (next == s_read_index) {
            ++s_ring_overrun_count;
        } else {
            s_ring[s_write_index] = s_dma_buffer[index];
            s_write_index = next;
        }
    }
    h723_k230_start_receive();
}

void h723_k230_on_uart2_error(void)
{
    ++s_uart_error_count;
    h723_k230_start_receive();
}

void h723_k230_service_step(uint32_t now_ms)
{
    while (s_read_index != s_write_index) {
        (void)app_k230_parser_feed(&s_parser, s_ring[s_read_index], now_ms, &s_sample);
        s_read_index = (uint16_t)((s_read_index + 1U) % H723_K230_RING_SIZE);
    }
    h723_k230_publish_debug(now_ms);
}

#else

void h723_k230_service_init(void) {}
void h723_k230_service_step(uint32_t now_ms) { (void)now_ms; }
void h723_k230_on_uart2_rx_event(uint16_t size) { (void)size; }
void h723_k230_on_uart2_error(void) {}

#endif
