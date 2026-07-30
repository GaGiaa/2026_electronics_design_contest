#ifndef APP_DEBUG_H
#define APP_DEBUG_H

#include <stdint.h>

#include "app_crsf.h"

/* Keil Watch snapshot; single_motor input fields are the explicit debug control interface. */
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
    float left_target_output_speed_rpm;
    float right_target_output_speed_rpm;
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
    int16_t rotor_speed_rpm;
    int16_t feedback_current_raw;
    float feedback_output_speed_rpm;
    float feedback_current_A;
    uint8_t temperature_celsius;
    uint32_t feedback_age_ms;
    float target_output_speed_rpm;
    float pid_raw_output_A;
    float pid_p_out_A;
    float pid_i_out_A;
    float pid_d_out_A;
    float pid_output_A;
    int16_t commanded_current_raw;
    float commanded_current_A;
} h723_m2006_debug_t;

typedef struct {
    /* Watch inputs. Set enable to 1 only with the chassis lifted safely. */
    uint32_t enable;
    uint32_t control_mode;
    uint32_t selected_id;
    uint32_t default_id;
    float target_output_speed_rpm;
    /* Runtime target limit in output-shaft RPM; initialized from the config default. */
    float max_target_output_speed_rpm;
    float kp;
    float ki;
    float kd;
    float output_limit;
    float deadband;
    float integral_output_limit;
    float integral_separation_threshold;
    float derivative_filter_N;
    float output_delta_limit;
    float target_position_deg;
    float position_kp;
    float position_ki;
    float position_kd;
    float position_output_limit_rpm;
    float position_deadband_deg;
    /* Program outputs and selected-motor feedback. */
    uint32_t active;
    uint32_t reset_pid;
    uint32_t safety_reason;
    uint32_t cycle_count;
    uint32_t position_cycle_count;
    uint32_t position_reference_valid;
    uint16_t feedback_encoder;
    int16_t rotor_speed_rpm;
    int16_t feedback_current_raw;
    float feedback_output_speed_rpm;
    float feedback_current_A;
    uint8_t feedback_temperature_celsius;
    uint32_t feedback_age_ms;
    float feedback_position_deg;
    float position_target_output_speed_rpm;
    float position_p_out_rpm;
    float position_i_out_rpm;
    float position_d_out_rpm;
    float position_output_rpm;
    int16_t target_current_raw;
    float target_current_A;
    float pid_raw_output_A;
    float pid_p_out_A;
    float pid_i_out_A;
    float pid_d_out_A;
    float pid_output_A;
} h723_debug_single_motor_t;

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
    uint16_t raw[8];
    uint16_t normalized[8];
    uint8_t digital;
    uint8_t black_mask;
    uint8_t adc_timeout_mask;
    uint8_t black_count;
    uint32_t line_strength;
    int32_t line_error;
    uint32_t sequence;
    uint32_t adc_timeout_count;
} h723_debug_grayscale_t;

typedef struct {
    h723_debug_system_t system;
    h723_debug_uart8_t uart8;
    h723_debug_crsf_t crsf;
    h723_debug_chassis_t chassis;
    h723_debug_fdcan_t fdcan;
    /* Index 0/1/2 maps to M2006 CAN ID 1/2/3. */
    h723_m2006_debug_t m2006[3];
    h723_debug_single_motor_t single_motor;
    h723_debug_jy901s_t jy901s;
    h723_debug_grayscale_t grayscale;
} h723_debug_t;

extern volatile h723_debug_t g_h723_debug;

#endif
