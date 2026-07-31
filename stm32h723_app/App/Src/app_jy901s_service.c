#include "app_jy901s_service.h"

#include <limits.h>

#include "app_debug.h"
#include "app_jy901s.h"
#include "app_jy901s_calibration.h"
#include "app_config.h"
#include "app_time.h"
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
static app_jy901s_calibration_t s_calibration;
static app_jy901s_calibration_output_t s_calibration_output;

static void h723_jy901s_calibration_init(uint32_t now_ms)
{
    const app_jy901s_calibration_config_t config = {
        .enable_gyro_calibration = APP_JY901S_CALIBRATION_ENABLE != 0U,
        .vehicle_axis_sensor = {
            APP_JY901S_VEHICLE_X_SENSOR_AXIS,
            APP_JY901S_VEHICLE_Y_SENSOR_AXIS,
            APP_JY901S_VEHICLE_Z_SENSOR_AXIS
        },
        .vehicle_axis_sign = {
            APP_JY901S_VEHICLE_X_SENSOR_SIGN,
            APP_JY901S_VEHICLE_Y_SENSOR_SIGN,
            APP_JY901S_VEHICLE_Z_SENSOR_SIGN
        },
        .angle_offset_deg = {
            APP_JY901S_ROLL_OFFSET_DEG,
            APP_JY901S_PITCH_OFFSET_DEG,
            APP_JY901S_YAW_OFFSET_DEG
        },
        .sample_target = APP_JY901S_CALIBRATION_SAMPLE_TARGET,
        .stationary_accel_min_g = APP_JY901S_CALIBRATION_STATIONARY_ACCEL_MIN_G,
        .stationary_accel_max_g = APP_JY901S_CALIBRATION_STATIONARY_ACCEL_MAX_G,
        .stationary_gyro_max_dps = APP_JY901S_CALIBRATION_STATIONARY_GYRO_MAX_DPS,
        .timeout_ms = APP_JY901S_CALIBRATION_TIMEOUT_MS
    };

    (void)app_jy901s_calibration_init(&s_calibration, &config, now_ms);
}

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
    for (index = 0U; index < 3U; ++index) {
        g_h723_debug.jy901s.vehicle_acceleration_g[index] = s_calibration_output.acceleration_g[index];
        g_h723_debug.jy901s.vehicle_angular_rate_dps[index] = s_calibration_output.angular_rate_dps[index];
        g_h723_debug.jy901s.vehicle_angle_deg[index] = s_calibration_output.angle_deg[index];
        g_h723_debug.jy901s.gyro_bias_dps[index] = s_calibration_output.gyro_bias_dps[index];
    }
    g_h723_debug.jy901s.calibration_status = s_calibration.status;
    g_h723_debug.jy901s.calibration_reason = s_calibration.reason;
    g_h723_debug.jy901s.calibration_sample_count = s_calibration.calibration_sample_count;
    g_h723_debug.jy901s.calibration_valid = s_calibration.calibration_valid ? 1U : 0U;
}

void h723_jy901s_service_init(void)
{
    app_jy901s_parser_init(&s_parser);
    h723_jy901s_calibration_init(h723_app_time_now_ms());
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
        if (app_jy901s_parser_feed(&s_parser, s_ring[s_read_index], now_ms, &s_sample)) {
            (void)app_jy901s_calibration_update(&s_calibration, &s_sample, now_ms, &s_calibration_output);
        }
        s_read_index = (uint16_t)((s_read_index + 1U) % H723_JY901S_RING_SIZE);
    }
    h723_jy901s_publish_debug(now_ms);
}
