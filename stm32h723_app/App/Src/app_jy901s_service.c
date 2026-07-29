#include "app_jy901s_service.h"

#include <limits.h>

#include "app_debug.h"
#include "app_jy901s.h"
#include "usart.h"

#define H723_JY901S_DMA_BUFFER_SIZE 64U
#define H723_JY901S_RING_SIZE 256U

static uint8_t s_dma_buffer[H723_JY901S_DMA_BUFFER_SIZE];
static volatile uint8_t s_ring[H723_JY901S_RING_SIZE];
static volatile uint16_t s_write_index;
static uint16_t s_read_index;
static uint32_t s_uart_error_count;
static uint32_t s_ring_overrun_count;
static app_jy901s_parser_t s_parser;
static app_jy901s_sample_t s_sample;

static void h723_jy901s_start_receive(void)
{
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart9, s_dma_buffer, sizeof(s_dma_buffer)) == HAL_OK && huart9.hdmarx != NULL) {
        __HAL_DMA_DISABLE_IT(huart9.hdmarx, DMA_IT_HT);
    }
}

static void h723_jy901s_publish_debug(uint32_t now_ms)
{
    uint32_t index;

    for (index = 0U; index < 3U; ++index) {
        g_h723_debug.jy901s.acceleration_raw[index] = s_sample.acceleration_raw[index];
        g_h723_debug.jy901s.angular_rate_raw[index] = s_sample.angular_rate_raw[index];
        g_h723_debug.jy901s.angle_raw[index] = s_sample.angle_raw[index];
        g_h723_debug.jy901s.acceleration_g[index] = s_sample.acceleration_g[index];
        g_h723_debug.jy901s.angular_rate_dps[index] = s_sample.angular_rate_dps[index];
        g_h723_debug.jy901s.angle_deg[index] = s_sample.angle_deg[index];
    }
    g_h723_debug.jy901s.temperature_raw = s_sample.temperature_raw;
    g_h723_debug.jy901s.temperature_celsius = s_sample.temperature_celsius;
    g_h723_debug.jy901s.acceleration_frame_count = s_sample.acceleration_frame_count;
    g_h723_debug.jy901s.gyro_frame_count = s_sample.gyro_frame_count;
    g_h723_debug.jy901s.angle_frame_count = s_sample.angle_frame_count;
    g_h723_debug.jy901s.checksum_error_count = s_sample.checksum_error_count;
    g_h723_debug.jy901s.format_error_count = s_sample.format_error_count;
    g_h723_debug.jy901s.complete_sample_count = s_sample.complete_sample_count;
    g_h723_debug.jy901s.uart_error_count = s_uart_error_count;
    g_h723_debug.jy901s.ring_overrun_count = s_ring_overrun_count;
    g_h723_debug.jy901s.dma_active = huart9.RxState == HAL_UART_STATE_BUSY_RX ? 1U : 0U;
    g_h723_debug.jy901s.sample_valid = s_sample.valid ? 1U : 0U;
    g_h723_debug.jy901s.sample_age_ms = s_sample.valid ? (uint32_t)(now_ms - s_sample.last_sample_ms) : UINT_MAX;
}

void h723_jy901s_service_init(void)
{
    app_jy901s_parser_init(&s_parser);
    h723_jy901s_start_receive();
}

void h723_jy901s_on_uart9_rx_event(uint16_t size)
{
    uint16_t index;

    if (size > H723_JY901S_DMA_BUFFER_SIZE) { size = H723_JY901S_DMA_BUFFER_SIZE; }
    for (index = 0U; index < size; ++index) {
        uint16_t next = (uint16_t)((s_write_index + 1U) % H723_JY901S_RING_SIZE);
        if (next == s_read_index) { ++s_ring_overrun_count; }
        else { s_ring[s_write_index] = s_dma_buffer[index]; s_write_index = next; }
    }
    h723_jy901s_start_receive();
}

void h723_jy901s_on_uart9_error(void)
{
    ++s_uart_error_count;
    h723_jy901s_start_receive();
}

void h723_jy901s_service_step(uint32_t now_ms)
{
    while (s_read_index != s_write_index) {
        (void)app_jy901s_parser_feed(&s_parser, s_ring[s_read_index], now_ms, &s_sample);
        s_read_index = (uint16_t)((s_read_index + 1U) % H723_JY901S_RING_SIZE);
    }
    h723_jy901s_publish_debug(now_ms);
}
