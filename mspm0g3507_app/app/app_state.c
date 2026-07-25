#include "app/app_state.h"

#include <stddef.h>

#if CRSF_REMOTE_CONTROL_ENABLE
#include <FreeRTOS.h>
#include <task.h>
#endif

volatile board_encoder_sample_t g_encoder_samples[BOARD_MOTOR_COUNT];
volatile board_grayscale_snapshot_t g_grayscale_snapshot;
line_tracking_state_t g_line_tracking_state;
static volatile uint32_t g_encoder_sample_sequence;
static volatile uint32_t g_grayscale_publish_sequence;
#if APP_IMU_YAW_ENABLE
static volatile app_imu_yaw_snapshot_t g_imu_yaw_snapshot;
static volatile uint32_t g_imu_yaw_publish_sequence;
#endif

#if CRSF_REMOTE_CONTROL_ENABLE
volatile crsf_debug_state_t g_crsf_debug;
#endif

#if APP_SERVO_FEATURE_ENABLE
volatile uint32_t g_servo_angle_deg = APP_SERVO_INITIAL_ANGLE_DEG;
volatile uint32_t g_servo_pulse_us;
#endif

void app_state_init(void)
{
    uint32_t wheel;

    for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
        g_encoder_samples[wheel] = (board_encoder_sample_t){0};
    }
    g_grayscale_snapshot = (board_grayscale_snapshot_t){0};
    g_line_tracking_state = (line_tracking_state_t){0};
    g_encoder_sample_sequence = 0U;
    g_grayscale_publish_sequence = 0U;
#if APP_IMU_YAW_ENABLE
    g_imu_yaw_snapshot = (app_imu_yaw_snapshot_t){0};
    g_imu_yaw_publish_sequence = 0U;
#endif
#if CRSF_REMOTE_CONTROL_ENABLE
    g_crsf_debug = (crsf_debug_state_t){0};
#endif
#if APP_SERVO_FEATURE_ENABLE
    g_servo_angle_deg = APP_SERVO_INITIAL_ANGLE_DEG;
    g_servo_pulse_us = 0U;
#endif
}

void app_state_encoder_cycle_begin(void)
{
    ++g_encoder_sample_sequence;
}

void app_state_encoder_sample_publish(board_motor_wheel_t wheel,
                                      board_encoder_sample_t sample)
{
    if (wheel < BOARD_MOTOR_COUNT) {
        g_encoder_samples[wheel] = sample;
    }
}

void app_state_encoder_cycle_end(void)
{
    ++g_encoder_sample_sequence;
}

void app_state_encoder_samples_snapshot_copy(
    board_encoder_sample_t samples[BOARD_MOTOR_COUNT])
{
    uint32_t begin_sequence;
    uint32_t end_sequence;
    uint32_t wheel;

    if (samples == NULL) {
        return;
    }
    for (;;) {
        begin_sequence = g_encoder_sample_sequence;
        if ((begin_sequence & 1U) == 0U) {
            for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
                samples[wheel] = g_encoder_samples[wheel];
            }
            end_sequence = g_encoder_sample_sequence;
            if ((begin_sequence == end_sequence) && ((end_sequence & 1U) == 0U)) {
                break;
            }
        }
    }
}

void app_state_motor_control_snapshot_copy(
    motor_control_wheel_status_t control[BOARD_MOTOR_COUNT])
{
    uint32_t begin_sequence;
    uint32_t end_sequence;
    uint32_t wheel;

    if (control == NULL) {
        return;
    }
    for (;;) {
        begin_sequence = g_encoder_sample_sequence;
        if ((begin_sequence & 1U) == 0U) {
            for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
                control[wheel] = g_motor_control_status[wheel];
            }
            end_sequence = g_encoder_sample_sequence;
            if ((begin_sequence == end_sequence) && ((end_sequence & 1U) == 0U)) {
                break;
            }
        }
    }
}

void app_state_grayscale_publish(const board_grayscale_snapshot_t *snapshot)
{
    uint32_t channel;

    if (snapshot == NULL) {
        return;
    }
    ++g_grayscale_publish_sequence;
    for (channel = 0U; channel < BOARD_GRAYSCALE_CHANNEL_COUNT; ++channel) {
        g_grayscale_snapshot.raw[channel] = snapshot->raw[channel];
        g_grayscale_snapshot.normalized[channel] = snapshot->normalized[channel];
    }
    g_grayscale_snapshot.digital = snapshot->digital;
    g_grayscale_snapshot.black_mask = snapshot->black_mask;
    g_grayscale_snapshot.black_count = snapshot->black_count;
    g_grayscale_snapshot.line_strength = snapshot->line_strength;
    g_grayscale_snapshot.line_error = snapshot->line_error;
    g_grayscale_snapshot.sequence = snapshot->sequence;
    ++g_grayscale_publish_sequence;
}

void app_state_grayscale_snapshot_copy(board_grayscale_snapshot_t *snapshot)
{
    uint32_t begin_sequence;
    uint32_t end_sequence;
    uint32_t channel;

    if (snapshot == NULL) {
        return;
    }
    for (;;) {
        begin_sequence = g_grayscale_publish_sequence;
        if ((begin_sequence & 1U) == 0U) {
            for (channel = 0U; channel < BOARD_GRAYSCALE_CHANNEL_COUNT; ++channel) {
                snapshot->raw[channel] = g_grayscale_snapshot.raw[channel];
                snapshot->normalized[channel] = g_grayscale_snapshot.normalized[channel];
            }
            snapshot->digital = g_grayscale_snapshot.digital;
            snapshot->black_mask = g_grayscale_snapshot.black_mask;
            snapshot->black_count = g_grayscale_snapshot.black_count;
            snapshot->line_strength = g_grayscale_snapshot.line_strength;
            snapshot->line_error = g_grayscale_snapshot.line_error;
            snapshot->sequence = g_grayscale_snapshot.sequence;
            end_sequence = g_grayscale_publish_sequence;
            if ((begin_sequence == end_sequence) && ((end_sequence & 1U) == 0U)) {
                break;
            }
        }
    }
}

#if APP_IMU_YAW_ENABLE
void app_state_imu_yaw_publish(float yaw_deg, float yaw_rate_dps,
                               float gyro_bias_z_dps)
{
    ++g_imu_yaw_publish_sequence;
    g_imu_yaw_snapshot.yaw_deg = yaw_deg;
    g_imu_yaw_snapshot.yaw_rate_dps = yaw_rate_dps;
    g_imu_yaw_snapshot.gyro_bias_z_dps = gyro_bias_z_dps;
    g_imu_yaw_snapshot.valid = true;
    ++g_imu_yaw_snapshot.sequence;
    ++g_imu_yaw_publish_sequence;
}

void app_state_imu_yaw_invalidate(void)
{
    ++g_imu_yaw_publish_sequence;
    g_imu_yaw_snapshot.valid = false;
    ++g_imu_yaw_snapshot.sequence;
    ++g_imu_yaw_publish_sequence;
}

void app_state_imu_yaw_snapshot_copy(app_imu_yaw_snapshot_t *snapshot)
{
    uint32_t begin_sequence;
    uint32_t end_sequence;

    if (snapshot == NULL) {
        return;
    }
    for (;;) {
        begin_sequence = g_imu_yaw_publish_sequence;
        if ((begin_sequence & 1U) == 0U) {
            snapshot->yaw_deg = g_imu_yaw_snapshot.yaw_deg;
            snapshot->yaw_rate_dps = g_imu_yaw_snapshot.yaw_rate_dps;
            snapshot->gyro_bias_z_dps = g_imu_yaw_snapshot.gyro_bias_z_dps;
            snapshot->valid = g_imu_yaw_snapshot.valid;
            snapshot->sequence = g_imu_yaw_snapshot.sequence;
            end_sequence = g_imu_yaw_publish_sequence;
            if ((begin_sequence == end_sequence) && ((end_sequence & 1U) == 0U)) {
                break;
            }
        }
    }
}
#endif

#if CRSF_REMOTE_CONTROL_ENABLE
void app_state_crsf_snapshot_copy(crsf_control_input_t *input)
{
    uint32_t channel;

    if (input == NULL) {
        return;
    }
    taskENTER_CRITICAL();
    input->valid = g_crsf_debug.valid_frame_count != 0U;
    input->last_valid_time_ms = g_crsf_debug.last_valid_time_ms;
    for (channel = 0U; channel < CRSF_CHANNEL_COUNT; ++channel) {
        input->channels[channel] = g_crsf_debug.channels.channels[channel];
    }
    taskEXIT_CRITICAL();
}

void app_state_crsf_publish(const crsf_channels_t *channels,
                            uint32_t now_ms,
                            const crsf_parser_t *parser,
                            uint32_t rx_overflow_count)
{
    if ((channels == NULL) || (parser == NULL)) {
        return;
    }
    taskENTER_CRITICAL();
    g_crsf_debug.channels = *channels;
    g_crsf_debug.last_valid_time_ms = now_ms;
    ++g_crsf_debug.valid_frame_count;
    g_crsf_debug.crc_error_count = parser->crc_error_count;
    g_crsf_debug.frame_error_count = parser->frame_error_count;
    g_crsf_debug.rx_overflow_count = rx_overflow_count;
    taskEXIT_CRITICAL();
}

void app_state_crsf_set_link_active(bool active)
{
    g_crsf_debug.link_active = active;
}
#endif
