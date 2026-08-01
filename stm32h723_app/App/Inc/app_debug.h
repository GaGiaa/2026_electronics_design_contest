#ifndef APP_DEBUG_H
#define APP_DEBUG_H

#include <stdint.h>

#include "app_crsf.h"

/* Keil Watch snapshot; single_motor input fields are the explicit debug control interface. */
typedef struct {
    float pid_kp;
    float pid_ki;
    float pid_kd;
    float pid_output_limit_mm_s;
    float pid_deadband;
    uint32_t reset_pid_request;
    uint32_t params_valid;
    uint32_t params_rejected_count;
} h723_debug_line_follow_params_t;

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
    uint32_t tx_started_ms;
    uint32_t tx_timeout_count;
    uint32_t tx_recovery_count;
    uint32_t tx_recovery_failure_count;
    uint32_t hal_g_state;
    uint32_t hal_error_code;
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
    float base_speed_mm_s;
    float line_position;
    float line_turn_correction_mm_s;
    uint32_t line_valid;
    uint32_t line_strength;
    float left_target_speed_mm_s;
    float right_target_speed_mm_s;
    float left_target_output_speed_rpm;
    float right_target_output_speed_rpm;
} h723_debug_chassis_t;

typedef struct {
    /* Keil Watch inputs. Values are applied before the next PID calculation. */
    float pid_kp;
    float pid_ki;
    float pid_kd;
    float pid_output_limit_mm_s;
    float pid_deadband;
    uint32_t reset_pid_request;
    /* Runtime status and PID calculation snapshot. */
    uint32_t params_valid;
    uint32_t params_rejected_count;
    uint32_t active;
    uint32_t line_valid;
    uint32_t adc_timeout_mask;
    uint32_t line_strength;
    uint32_t sequence;
    uint32_t pid_update_count;
    float pid_dt_s;
    float target_position;
    float line_position;
    float error;
    float integral;
    float p_out;
    float i_out;
    float d_out;
    float raw_output;
    float pid_output;
    float turn_correction_mm_s;
    float base_speed_mm_s;
    float left_target_speed_mm_s;
    float right_target_speed_mm_s;
    uint32_t active_group;
} h723_debug_line_follow_t;

typedef struct {
    uint32_t mode;
    uint32_t remote_takeover;
    uint32_t buttons_enabled;
    uint32_t se_pressed;
    uint32_t sb_state;
    uint32_t sc_state;
    uint32_t button_stable_high_mask;
    uint32_t selected_task;
    uint32_t task_request_available;
    uint32_t active_task_elapsed_ms;
} h723_debug_control_t;

typedef struct {
    uint32_t phase;
    uint32_t fault;
    uint32_t running;
    uint32_t stop_mark;
    uint32_t elapsed_ms;
    float distance_mm;
    float base_speed_mm_s;
} h723_debug_task2_t;

typedef struct {
    uint32_t phase;
    uint32_t running;
    uint32_t elapsed_ms;
    float base_speed_mm_s;
} h723_debug_task4_t;

typedef struct {
    uint32_t task_id;
    uint32_t phase;
    uint32_t running;
    uint32_t elapsed_ms;
    float base_speed_mm_s;
} h723_debug_task56_t;

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
    /* Keil Watch inputs. `rehome_request` is consumed as a one-shot command. */
    float target_position_deg;
    uint32_t rehome_request;
    /* Set only after confirming the debug range and chassis are safe. */
    uint32_t allow_extended_position_range;
    /* Program state and feedback. */
    uint32_t state;
    uint32_t fault;
    uint32_t zero_valid;
    uint32_t target_clamped;
    uint32_t extended_position_range_active;
    uint32_t cycle_count;
    float active_target_position_deg;
    float zero_offset_deg;
    float feedback_position_deg;
    float feedback_output_speed_rpm;
    float feedback_current_a;
    float target_output_speed_rpm;
    float commanded_current_a;
    /* 平衡速度内环的实际配置与状态，供 Keil Watch 排查增量 PID。 */
    float speed_pid_kp;
    float speed_pid_ki;
    float speed_pid_kd;
    float speed_pid_dt_s;
    float speed_pid_error_rpm;
    float speed_pid_integral_output_a;
    float speed_pid_raw_output_a;
    float speed_pid_p_out_a;
    float speed_pid_i_out_a;
    float speed_pid_d_out_a;
    float speed_pid_output_a;
    int16_t commanded_current_raw;
} h723_debug_balance_t;

typedef struct {
    /* Keil Watch inputs. capture_zero_request is consumed as a one-shot command. */
    uint32_t enable;
    float target_tilt_deg;
    uint32_t capture_zero_request;
    float pid_kp;
    float pid_ki;
    float pid_kd;
    float derivative_filter_N;
    float pid_deadband_deg;
    float max_position_rate_deg_s;
    /* Runtime state and 100 Hz outer-loop snapshot. */
    uint32_t state;
    uint32_t fault;
    uint32_t capture_zero_consumed;
    uint32_t zero_captured_valid;
    uint32_t new_imu_sample;
    uint32_t motor_target_clamped;
    uint32_t imu_sample_age_ms;
    float raw_pitch_deg;
    float captured_zero_deg;
    float tilt_deg;
    float error_deg;
    float pid_rate_deg_s;
    float pid_p_out_deg_s;
    float pid_i_out_deg_s;
    float pid_d_out_deg_s;
    float pid_integral;
    float measured_tilt_rate_deg_s;
    float filtered_tilt_rate_deg_s;
    float motor_target_position_deg;
} h723_debug_tilt_t;

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
    float vehicle_acceleration_g[3];
    float vehicle_angular_rate_dps[3];
    float vehicle_angle_deg[3];
    float gyro_bias_dps[3];
    uint32_t calibration_status;
    uint32_t calibration_reason;
    uint32_t calibration_sample_count;
    uint32_t calibration_valid;
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
    float line_position;
    uint32_t sequence;
    uint32_t adc_timeout_count;
} h723_debug_grayscale_t;

typedef struct {
    uint32_t raw_high_mask;
    uint32_t stable_high_mask;
    uint32_t sample_sequence;
} h723_debug_buttons_t;

typedef struct {
    int16_t acceleration_raw[3];
    int16_t angular_rate_raw[3];
    int16_t angle_raw[3];
    int16_t temperature_raw;
    float acceleration_g[3];
    float angular_rate_dps[3];
    float angle_deg[3];
    float temperature_celsius;
    uint32_t calibration_sys;
    uint32_t calibration_gyro;
    uint32_t calibration_acc;
    uint32_t calibration_mag;
    uint32_t sys_status;
    uint32_t sys_err;
    uint32_t complete_sample_count;
    uint32_t start_attempt_count;
    uint32_t uart_error_count;
    int32_t last_error;
    uint32_t last_stage;
    uint32_t last_detail;
    int32_t transport_status;
    uint32_t hal_error_code;
    uint32_t last_rx0;
    uint32_t last_rx1;
    uint32_t sample_age_ms;
    uint32_t sample_valid;
    uint32_t online;
} h723_debug_bno055_t;

typedef struct {
    float distance_mm;
    uint32_t valid;
    uint32_t valid_frame_count;
    uint32_t crc_error_count;
    uint32_t format_error_count;
    uint32_t uart_error_count;
    uint32_t ring_overrun_count;
    uint32_t frame_age_ms;
    uint32_t dma_active;
} h723_debug_ball_vision_t;

typedef struct {
    /* Keil Watch inputs. Target and feedback share the K230 millimetre frame. */
    uint32_t enable;
    float target_mm;
    float pid_kp;
    float pid_ki;
    float pid_kd;
    float output_limit_deg;
    float pid_deadband_mm;
    float hold_position_mm[3];
    float hold_tilt_deg[3];
    float engage_error_mm;
    float release_error_mm;
    float breakaway_positive_tilt_deg;
    float breakaway_negative_tilt_deg;
    float velocity_gain_deg_per_mm_s;
    float velocity_filter_alpha;
    /* Runtime state. */
    uint32_t state;
    uint32_t fault;
    uint32_t update_due;
    uint32_t vision_valid;
    uint32_t vision_frame_count;
    uint32_t vision_age_ms;
    float measured_mm;
    float error_mm;
    float pid_p_out_deg;
    float pid_i_out_deg;
    float pid_d_out_deg;
    float pid_output_deg;
    float target_tilt_deg;
    float pid_integral;
    uint32_t drive_active;
    float hold_tilt_output_deg;
    float breakaway_tilt_output_deg;
    float velocity_mm_s;
    float velocity_damping_tilt_deg;
} h723_debug_ball_position_t;

typedef struct {
    uint32_t state;
    uint32_t calibration_valid;
    uint32_t id3_allowed;
    uint32_t other_motors_allowed;
    float captured_pitch_deg;
} h723_debug_pipe_startup_t;

typedef struct {
    uint32_t enabled;
    uint32_t initialized;
    uint32_t init_attempt_count;
    uint32_t last_hal_status;
    /* I2C4 transfer diagnostics, sampled after each OLED service attempt. */
    uint32_t i2c_error_code;
    uint32_t i2c_state;
    uint32_t i2c_isr;
    uint32_t gpio_pd_idr;
    uint32_t i2c_recovery_count;
    uint32_t i2c_recovery_status;
    uint32_t update_count;
    uint32_t error_count;
} h723_debug_oled_t;

typedef struct {
    h723_debug_system_t system;
    h723_debug_uart8_t uart8;
    h723_debug_crsf_t crsf;
    h723_debug_control_t control;
    h723_debug_task2_t task2;
    h723_debug_task4_t task4;
    h723_debug_task56_t task56;
    h723_debug_chassis_t chassis;
    h723_debug_line_follow_params_t task2_line_follow;
    h723_debug_line_follow_params_t task456_line_follow;
    h723_debug_line_follow_t line_follow;
    h723_debug_fdcan_t fdcan;
    /* Index 0/1/2 maps to M2006 CAN ID 1/2/3. */
    h723_m2006_debug_t m2006[3];
    h723_debug_single_motor_t single_motor;
    h723_debug_balance_t balance;
    h723_debug_tilt_t tilt;
    h723_debug_jy901s_t jy901s;
    h723_debug_grayscale_t grayscale;
    h723_debug_buttons_t buttons;
    h723_debug_bno055_t bno055;
    h723_debug_ball_vision_t ball_vision;
    h723_debug_ball_position_t ball_position;
    h723_debug_pipe_startup_t pipe_startup;
    h723_debug_oled_t oled;
} h723_debug_t;

extern volatile h723_debug_t g_h723_debug;

#endif
