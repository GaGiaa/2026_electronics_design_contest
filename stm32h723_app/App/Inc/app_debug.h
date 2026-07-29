#ifndef APP_DEBUG_H
#define APP_DEBUG_H

#include <stdbool.h>
#include <stdint.h>

#include "app_crsf.h"

typedef struct {
    uint16_t feedback_id;
    uint16_t encoder;
    int16_t feedback_speed_rpm;
    int16_t feedback_current;
    uint8_t temperature_celsius;
    uint32_t feedback_age_ms;
    float target_speed_rpm;
    float pid_p_out;
    float pid_i_out;
    float pid_d_out;
    float pid_output;
    int16_t commanded_current;
} h723_m2006_debug_t;

typedef struct {
    uint32_t boot_count;
    uint32_t uptime_ms;
    uint32_t task_loop_count;
    uint32_t telemetry_enabled;
    uint32_t tx_start_count;
    uint32_t tx_complete_count;
    uint32_t tx_drop_count;
    uint32_t last_hal_status;
    uint32_t tx_in_flight;
    uint16_t crsf_channels_raw[APP_CRSF_CHANNEL_COUNT];
    uint32_t crsf_valid_frame_count;
    uint32_t crsf_crc_error_count;
    uint32_t crsf_frame_error_count;
    uint32_t crsf_uart_error_count;
    uint32_t crsf_ring_overrun_count;
    uint32_t crsf_timeout_count;
    uint32_t crsf_sb_state;
    uint32_t crsf_sc_state;
    uint32_t crsf_age_ms;
    uint32_t chassis_mode;
    uint32_t chassis_actuation_enabled;
    float chassis_forward_normalized;
    float chassis_turn_normalized;
    float chassis_left_target_rpm;
    float chassis_right_target_rpm;
    uint32_t fdcan_rx_count;
    uint32_t fdcan_tx_count;
    uint32_t fdcan_tx_error_count;
    uint32_t fdcan_last_status;
    uint32_t fdcan_instance;
    uint32_t fdcan_protocol_last_error;
    uint32_t fdcan_protocol_activity;
    uint32_t fdcan_protocol_bus_off;
    uint32_t fdcan_tx_error_counter;
    uint32_t fdcan_rx_error_counter;
    h723_m2006_debug_t m2006[2];
    int16_t jy901s_acceleration_raw[3];
    int16_t jy901s_temperature_raw;
    int16_t jy901s_angular_rate_raw[3];
    int16_t jy901s_angle_raw[3];
    float jy901s_acceleration_g[3];
    float jy901s_temperature_celsius;
    float jy901s_angular_rate_dps[3];
    float jy901s_angle_deg[3];
    uint32_t jy901s_acceleration_frame_count;
    uint32_t jy901s_gyro_frame_count;
    uint32_t jy901s_angle_frame_count;
    uint32_t jy901s_checksum_error_count;
    uint32_t jy901s_format_error_count;
    uint32_t jy901s_complete_sample_count;
    uint32_t jy901s_uart_error_count;
    uint32_t jy901s_ring_overrun_count;
    uint32_t jy901s_sample_age_ms;
    uint32_t jy901s_sample_valid;
    uint32_t jy901s_dma_active;
} h723_debug_t;

extern volatile h723_debug_t g_h723_debug;

#endif
