#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "config/app_config.h"
#include "algorithms/line_control/line_control.h"
#include "algorithms/line_tracking/line_tracking.h"
#include "algorithms/motor_control/motor_control.h"
#include "drivers/encoder/board_encoder.h"
#include "drivers/grayscale/board_grayscale.h"
#include "protocols/crsf/crsf_control.h"
#include "protocols/crsf/crsf_protocol.h"

typedef struct {
    /* 当前 CRSF/循迹底盘模式。 */
    crsf_drive_mode_t mode;
    /* CRSF 帧链路是否在超时窗口内。 */
    bool link_active;
    /* SB/SC 原始通道值和灰度线控观察量。 */
    uint16_t sb_raw;
    uint16_t sc_raw;
    int32_t line_error;
    uint32_t line_strength;
    uint8_t adc_timeout_mask;
    bool line_valid;
    uint32_t lost_line_ms;
    /* 线控外环和四轮速度环的输入/输出，单位为 mm/s。 */
    float base_speed_mm_per_s;
    float turn_speed_mm_per_s;
    float pid_p_out;
    float pid_i_out;
    float pid_d_out;
    float pid_output;
    bool yaw_valid;
    float yaw_target_deg;
    float yaw_feedback_deg;
    float yaw_error_deg;
    float wheel_targets_mm_per_s[BOARD_MOTOR_COUNT];
    float wheel_feedback_mm_per_s[BOARD_MOTOR_COUNT];
    uint32_t line_sequence;
    uint32_t control_sequence;
} app_drive_control_snapshot_t;

typedef struct {
    uint32_t pressed_mask;
    uint32_t pressed_edge_mask;
    uint32_t released_edge_mask;
    uint32_t sequence;
} app_button_snapshot_t;

/* SWD-observable app-state sequence guards. Treat these as read-only. */
extern volatile uint32_t g_encoder_sample_sequence;
extern volatile uint32_t g_grayscale_publish_sequence;
extern volatile uint32_t g_drive_control_publish_sequence;

extern volatile board_encoder_sample_t g_encoder_samples[BOARD_MOTOR_COUNT];
extern volatile board_grayscale_snapshot_t g_grayscale_snapshot;
extern volatile app_drive_control_snapshot_t g_drive_control_snapshot;
extern volatile app_button_snapshot_t g_button_snapshot;
extern volatile uint32_t g_button_publish_sequence;
extern line_tracking_state_t g_line_tracking_state;

#if APP_IMU_YAW_ENABLE
typedef struct {
    float yaw_deg;
    float yaw_rate_dps;
    float gyro_bias_z_dps;
    bool valid;
    uint32_t sequence;
} app_imu_yaw_snapshot_t;

extern volatile app_imu_yaw_snapshot_t g_imu_yaw_snapshot;
extern volatile uint32_t g_imu_yaw_publish_sequence;
#endif

#if CRSF_REMOTE_CONTROL_ENABLE
typedef struct {
    crsf_channels_t channels;
    bool link_active;
    uint32_t last_valid_time_ms;
    uint32_t valid_frame_count;
    uint32_t crc_error_count;
    uint32_t frame_error_count;
    uint32_t rx_overflow_count;
} crsf_debug_state_t;

extern volatile crsf_debug_state_t g_crsf_debug;
#endif

#if APP_SERVO_FEATURE_ENABLE
extern volatile uint32_t g_servo_angle_deg;
extern volatile uint32_t g_servo_pulse_us;
#endif

void app_state_init(void);
void app_state_encoder_cycle_begin(void);
void app_state_encoder_sample_publish(board_motor_wheel_t wheel,
                                      board_encoder_sample_t sample);
void app_state_encoder_cycle_end(void);
void app_state_encoder_samples_snapshot_copy(
    board_encoder_sample_t samples[BOARD_MOTOR_COUNT]);
void app_state_motor_control_snapshot_copy(
    motor_control_wheel_status_t control[BOARD_MOTOR_COUNT]);
void app_state_grayscale_publish(const board_grayscale_snapshot_t *snapshot);
void app_state_grayscale_snapshot_copy(board_grayscale_snapshot_t *snapshot);
void app_state_drive_control_publish(
    const app_drive_control_snapshot_t *snapshot);
void app_state_drive_control_snapshot_copy(
    app_drive_control_snapshot_t *snapshot);
void app_state_buttons_publish(uint32_t pressed_mask,
                               uint32_t pressed_edge_mask,
                               uint32_t released_edge_mask);
void app_state_buttons_snapshot_copy(app_button_snapshot_t *snapshot);

#if APP_IMU_YAW_ENABLE
void app_state_imu_yaw_publish(float yaw_deg, float yaw_rate_dps,
                               float gyro_bias_z_dps);
void app_state_imu_yaw_invalidate(void);
void app_state_imu_yaw_snapshot_copy(app_imu_yaw_snapshot_t *snapshot);
#endif

#if CRSF_REMOTE_CONTROL_ENABLE
void app_state_crsf_snapshot_copy(crsf_control_input_t *input);
void app_state_crsf_publish(const crsf_channels_t *channels,
                            uint32_t now_ms,
                            const crsf_parser_t *parser,
                            uint32_t rx_overflow_count);
void app_state_crsf_set_link_active(bool active);
#endif

#endif
