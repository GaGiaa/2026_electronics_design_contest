#ifndef APP_DEBUG_H
#define APP_DEBUG_H

#include <stdint.h>

#include "app_crsf.h"

/* Keil Watch-only snapshot. Application control must not read these fields. */
typedef struct {
    uint32_t boot_count;
    uint32_t uptime_ms;
    uint32_t task_loop_count;
} h723_debug_system_t;

typedef struct {
    uint32_t telemetry_enabled;
    uint32_t tx_start_count;
    uint32_t tx_complete_count;
    uint32_t tx_drop_count;
    uint32_t last_hal_status;
    uint32_t tx_in_flight;
} h723_debug_uart8_t;

typedef struct {
    uint16_t channels_raw[APP_CRSF_CHANNEL_COUNT];
    uint32_t valid_frame_count;
    uint32_t crc_error_count;
    uint32_t frame_error_count;
    uint32_t uart_error_count;
    uint32_t ring_overrun_count;
    uint32_t timeout_count;
    uint32_t sb_state;
    uint32_t sc_state;
    uint32_t age_ms;
} h723_debug_crsf_t;

typedef struct {
    uint32_t mode;
    uint32_t actuation_enabled;
    float forward_normalized;
    float turn_normalized;
    float left_target_rpm;
    float right_target_rpm;
} h723_debug_chassis_t;

typedef struct {
    uint32_t rx_count;
    uint32_t tx_count;
    uint32_t tx_error_count;
    uint32_t last_status;
    uint32_t instance;
    uint32_t protocol_last_error;
    uint32_t protocol_activity;
    uint32_t protocol_bus_off;
    uint32_t tx_error_counter;
    uint32_t rx_error_counter;
} h723_debug_fdcan_t;

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
    int16_t acceleration_raw[3];
    int16_t temperature_raw;
    int16_t angular_rate_raw[3];
    int16_t angle_raw[3];
    float acceleration_g[3];
    float temperature_celsius;
    float angular_rate_dps[3];
    float angle_deg[3];
    uint32_t acceleration_frame_count;
    uint32_t gyro_frame_count;
    uint32_t angle_frame_count;
    uint32_t checksum_error_count;
    uint32_t format_error_count;
    uint32_t complete_sample_count;
    uint32_t uart_error_count;
    uint32_t ring_overrun_count;
    uint32_t sample_age_ms;
    uint32_t sample_valid;
    uint32_t dma_active;
} h723_debug_jy901s_t;

typedef struct {
    h723_debug_system_t system;
    h723_debug_uart8_t uart8;
    h723_debug_crsf_t crsf;
    h723_debug_chassis_t chassis;
    h723_debug_fdcan_t fdcan;
    /* Index 0/1/2 maps to M2006 CAN ID 1/2/3. */
    h723_m2006_debug_t m2006[3];
    h723_debug_jy901s_t jy901s;
} h723_debug_t;

extern volatile h723_debug_t g_h723_debug;

#endif
