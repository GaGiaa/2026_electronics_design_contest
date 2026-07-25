#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "config/app_config.h"
#include "algorithms/line_tracking/line_tracking.h"
#include "algorithms/motor_control/motor_control.h"
#include "drivers/encoder/board_encoder.h"
#include "drivers/grayscale/board_grayscale.h"
#include "protocols/crsf/crsf_control.h"
#include "protocols/crsf/crsf_protocol.h"

extern volatile board_encoder_sample_t g_encoder_samples[BOARD_MOTOR_COUNT];
extern volatile board_grayscale_snapshot_t g_grayscale_snapshot;
extern line_tracking_state_t g_line_tracking_state;

#if APP_IMU_YAW_ENABLE
typedef struct {
    float yaw_deg;
    float yaw_rate_dps;
    float gyro_bias_z_dps;
    bool valid;
    uint32_t sequence;
} app_imu_yaw_snapshot_t;
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
