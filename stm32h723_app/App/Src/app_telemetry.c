#include "app_telemetry.h"

#include "app_config.h"
#include "app_buttons.h"
#include "app_debug.h"
#include "app_single_motor.h"
#include "app_time.h"
#include "app_telemetry_tx_guard.h"
#include "usart.h"
#include "vofa_justfloat.h"

#define H723_VOFA_HEALTH_CHANNEL_COUNT 6U
#define H723_VOFA_HEALTH_MAGIC 723.0f
#define H723_VOFA_JY901S_CHANNEL_COUNT 10U
#define H723_VOFA_GRAYSCALE_CHANNEL_COUNT 22U
#define H723_VOFA_SINGLE_MOTOR_SPEED_CHANNEL_COUNT 8U
#define H723_VOFA_SINGLE_MOTOR_POSITION_CHANNEL_COUNT 9U
#define H723_VOFA_SINGLE_MOTOR_MAX_CHANNEL_COUNT H723_VOFA_SINGLE_MOTOR_POSITION_CHANNEL_COUNT
#define H723_VOFA_BNO055_CHANNEL_COUNT 10U
#define H723_VOFA_K230_CHANNEL_COUNT 3U
#define H723_VOFA_CHASSIS_CHANNEL_COUNT 5U
#define H723_VOFA_LINE_FOLLOW_PID_CHANNEL_COUNT 13U
#define H723_VOFA_BUTTON_CHANNEL_COUNT 3U
#define H723_VOFA_TILT_CONTROL_CHANNEL_COUNT 13U

static volatile app_telemetry_tx_guard_t s_uart8_tx_guard;

static void h723_uart8_publish_hal_state(void)
{
    g_h723_debug.uart8.hal_g_state = (uint32_t)huart8.gState;
    g_h723_debug.uart8.hal_error_code = huart8.ErrorCode;
}

static bool h723_uart8_try_transmit(uint8_t *frame, uint16_t frame_size, uint32_t now_ms)
{
    HAL_StatusTypeDef status;

    if (!app_telemetry_tx_guard_reserve(&s_uart8_tx_guard, now_ms)) {
        g_h723_debug.uart8.tx_drop_count++;
        return false;
    }
    g_h723_debug.uart8.tx_in_flight = 1U;
    g_h723_debug.uart8.tx_started_ms = now_ms;
    __DMB();
    status = HAL_UART_Transmit_DMA(&huart8, frame, frame_size);
    g_h723_debug.uart8.last_hal_status = (uint32_t)status;
    h723_uart8_publish_hal_state();
    if (status == HAL_OK) {
        g_h723_debug.uart8.tx_start_count++;
        return true;
    }

    app_telemetry_tx_guard_start_failed(&s_uart8_tx_guard);
    g_h723_debug.uart8.tx_in_flight = 0U;
    g_h723_debug.uart8.tx_drop_count++;
    return false;
}

static void h723_uart8_recover_timed_out_transfer(uint32_t now_ms)
{
    HAL_StatusTypeDef status;

    if (!app_telemetry_tx_guard_timed_out(&s_uart8_tx_guard, now_ms,
                                          APP_H723_UART8_TX_TIMEOUT_MS)) {
        return;
    }
    g_h723_debug.uart8.tx_timeout_count++;
    status = HAL_UART_AbortTransmit(&huart8);
    g_h723_debug.uart8.last_hal_status = (uint32_t)status;
    h723_uart8_publish_hal_state();
    if (status == HAL_OK) {
        app_telemetry_tx_guard_complete(&s_uart8_tx_guard);
        g_h723_debug.uart8.tx_in_flight = 0U;
        g_h723_debug.uart8.tx_recovery_count++;
    } else {
        g_h723_debug.uart8.tx_recovery_failure_count++;
    }
}

#if (APP_VOFA_HEALTH_TELEMETRY_ENABLE == 1U)
static uint8_t s_health_frame[VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_HEALTH_CHANNEL_COUNT)];
static uint32_t s_last_telemetry_ms;
#endif

#if (APP_BNO055_VOFA_TELEMETRY_ENABLE == 1U)
static uint8_t s_bno055_frame[VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_BNO055_CHANNEL_COUNT)];
static uint32_t s_last_bno055_telemetry_ms;
#endif

#if (APP_JY901S_VOFA_TELEMETRY_ENABLE == 1U)
static uint8_t s_jy901s_frame[VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_JY901S_CHANNEL_COUNT)];
static uint32_t s_last_jy901s_telemetry_ms;
#endif

#if (APP_GRAYSCALE_VOFA_TELEMETRY_ENABLE == 1U)
static uint8_t s_grayscale_frame[VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_GRAYSCALE_CHANNEL_COUNT)];
static uint32_t s_last_grayscale_telemetry_ms;
#endif

#if (APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_ENABLE == 1U)
static uint8_t s_single_motor_frame[VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_SINGLE_MOTOR_MAX_CHANNEL_COUNT)];
static uint32_t s_last_single_motor_telemetry_ms;
#endif

#if (APP_H723_K230_UART2_TEST_ENABLE == 1U)
static uint8_t s_k230_frame[VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_K230_CHANNEL_COUNT)];
static uint32_t s_last_k230_telemetry_ms;
#endif

#if (APP_H723_CHASSIS_VOFA_TELEMETRY_ENABLE == 1U)
static uint8_t s_chassis_frame[VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_CHASSIS_CHANNEL_COUNT)];
static uint32_t s_last_chassis_telemetry_ms;
#endif

#if (APP_H723_LINE_FOLLOW_PID_VOFA_TELEMETRY_ENABLE == 1U)
static uint8_t s_line_follow_pid_frame[
    VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_LINE_FOLLOW_PID_CHANNEL_COUNT)];
static uint32_t s_last_line_follow_pid_telemetry_ms;
#endif

#if (APP_H723_BUTTON_VOFA_TELEMETRY_ENABLE == 1U)
static uint8_t s_button_frame[VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_BUTTON_CHANNEL_COUNT)];
static uint32_t s_last_button_telemetry_ms;
#endif

#if (APP_H723_TILT_CONTROL_VOFA_TELEMETRY_ENABLE == 1U)
static uint8_t s_tilt_control_frame[
    VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_TILT_CONTROL_CHANNEL_COUNT)];
static uint32_t s_last_tilt_control_telemetry_ms;
#endif

void h723_app_telemetry_init(void)
{
    g_h723_debug.system.boot_count++;
    app_telemetry_tx_guard_init(&s_uart8_tx_guard);
    g_h723_debug.uart8.telemetry_enabled = APP_VOFA_HEALTH_TELEMETRY_ENABLE |
        APP_JY901S_VOFA_TELEMETRY_ENABLE |
        (APP_GRAYSCALE_VOFA_TELEMETRY_ENABLE << 1U) |
        (APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_ENABLE << 2U) |
        (APP_H723_K230_UART2_TEST_ENABLE << 3U) |
        (APP_H723_CHASSIS_VOFA_TELEMETRY_ENABLE << 4U) |
        (APP_H723_LINE_FOLLOW_PID_VOFA_TELEMETRY_ENABLE << 5U) |
        (APP_H723_BUTTON_VOFA_TELEMETRY_ENABLE << 4U) |
        (APP_H723_TILT_CONTROL_VOFA_TELEMETRY_ENABLE << 6U);
    g_h723_debug.uart8.last_hal_status = HAL_OK;
    g_h723_debug.uart8.tx_in_flight = 0U;
    g_h723_debug.uart8.tx_started_ms = 0U;
    g_h723_debug.uart8.tx_timeout_count = 0U;
    g_h723_debug.uart8.tx_recovery_count = 0U;
    g_h723_debug.uart8.tx_recovery_failure_count = 0U;
    h723_uart8_publish_hal_state();
#if (APP_VOFA_HEALTH_TELEMETRY_ENABLE == 1U)
    s_last_telemetry_ms = h723_app_time_now_ms();
#endif
#if (APP_BNO055_VOFA_TELEMETRY_ENABLE == 1U)
    s_last_bno055_telemetry_ms = h723_app_time_now_ms();
#endif
#if (APP_JY901S_VOFA_TELEMETRY_ENABLE == 1U)
    s_last_jy901s_telemetry_ms = h723_app_time_now_ms();
#endif
#if (APP_GRAYSCALE_VOFA_TELEMETRY_ENABLE == 1U)
    s_last_grayscale_telemetry_ms = h723_app_time_now_ms();
#endif

#if (APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_ENABLE == 1U)
    s_last_single_motor_telemetry_ms = h723_app_time_now_ms();
#endif
#if (APP_H723_K230_UART2_TEST_ENABLE == 1U)
    s_last_k230_telemetry_ms = h723_app_time_now_ms();
#endif
#if (APP_H723_CHASSIS_VOFA_TELEMETRY_ENABLE == 1U)
    s_last_chassis_telemetry_ms = h723_app_time_now_ms();
#endif
#if (APP_H723_LINE_FOLLOW_PID_VOFA_TELEMETRY_ENABLE == 1U)
    s_last_line_follow_pid_telemetry_ms = h723_app_time_now_ms();
#endif
#if (APP_H723_BUTTON_VOFA_TELEMETRY_ENABLE == 1U)
    s_last_button_telemetry_ms = h723_app_time_now_ms();
#endif
#if (APP_H723_TILT_CONTROL_VOFA_TELEMETRY_ENABLE == 1U)
    s_last_tilt_control_telemetry_ms = h723_app_time_now_ms();
#endif
}

void h723_app_telemetry_step(void)
{
    uint32_t now_ms = h723_app_time_now_ms();

    g_h723_debug.system.uptime_ms = now_ms;
    g_h723_debug.system.task_loop_count++;
    h723_uart8_publish_hal_state();
    h723_uart8_recover_timed_out_transfer(now_ms);

#if (APP_VOFA_HEALTH_TELEMETRY_ENABLE == 1U)
    if ((now_ms - s_last_telemetry_ms) >= APP_VOFA_HEALTH_TELEMETRY_INTERVAL_MS) {
        const float channels[H723_VOFA_HEALTH_CHANNEL_COUNT] = {
            H723_VOFA_HEALTH_MAGIC,
            (float)g_h723_debug.system.uptime_ms,
            (float)g_h723_debug.system.task_loop_count,
            (float)g_h723_debug.uart8.tx_start_count,
            (float)g_h723_debug.uart8.tx_complete_count,
            (float)g_h723_debug.uart8.tx_drop_count
        };
        s_last_telemetry_ms = now_ms;
        (void)vofa_justfloat_encode(s_health_frame, sizeof(s_health_frame), channels,
                                    H723_VOFA_HEALTH_CHANNEL_COUNT);
        if (!h723_uart8_try_transmit(s_health_frame, sizeof(s_health_frame), now_ms)) {
            return;
        }
    }
#endif

#if (APP_JY901S_VOFA_TELEMETRY_ENABLE == 1U)
    if (g_h723_debug.jy901s.sample_valid != 0U &&
#if (APP_JY901S_VOFA_CALIBRATED_ENABLE == 1U)
        g_h723_debug.jy901s.calibration_valid != 0U &&
#endif
        (now_ms - s_last_jy901s_telemetry_ms) >= APP_JY901S_VOFA_TELEMETRY_INTERVAL_MS) {
#if (APP_JY901S_VOFA_CALIBRATED_ENABLE == 1U)
        const float channels[H723_VOFA_JY901S_CHANNEL_COUNT] = {
            g_h723_debug.jy901s.vehicle_acceleration_g[0],
            g_h723_debug.jy901s.vehicle_acceleration_g[1],
            g_h723_debug.jy901s.vehicle_acceleration_g[2],
            g_h723_debug.jy901s.vehicle_angular_rate_dps[0],
            g_h723_debug.jy901s.vehicle_angular_rate_dps[1],
            g_h723_debug.jy901s.vehicle_angular_rate_dps[2],
            g_h723_debug.jy901s.vehicle_angle_deg[0],
            g_h723_debug.jy901s.vehicle_angle_deg[1],
            g_h723_debug.jy901s.vehicle_angle_deg[2],
            g_h723_debug.jy901s.temperature_celsius
        };
#else
        const float channels[H723_VOFA_JY901S_CHANNEL_COUNT] = {
            g_h723_debug.jy901s.acceleration_g[0],
            g_h723_debug.jy901s.acceleration_g[1],
            g_h723_debug.jy901s.acceleration_g[2],
            g_h723_debug.jy901s.angular_rate_dps[0],
            g_h723_debug.jy901s.angular_rate_dps[1],
            g_h723_debug.jy901s.angular_rate_dps[2],
            g_h723_debug.jy901s.angle_deg[0],
            g_h723_debug.jy901s.angle_deg[1],
            g_h723_debug.jy901s.angle_deg[2],
            g_h723_debug.jy901s.temperature_celsius
        };
#endif
        s_last_jy901s_telemetry_ms = now_ms;
        (void)vofa_justfloat_encode(s_jy901s_frame, sizeof(s_jy901s_frame), channels,
                                    H723_VOFA_JY901S_CHANNEL_COUNT);
        if (!h723_uart8_try_transmit(s_jy901s_frame, sizeof(s_jy901s_frame), now_ms)) {
            return;
        }
    }
#endif

#if (APP_BNO055_VOFA_TELEMETRY_ENABLE == 1U)
    if (g_h723_debug.bno055.sample_valid != 0U &&
        (now_ms - s_last_bno055_telemetry_ms) >= APP_BNO055_VOFA_TELEMETRY_INTERVAL_MS) {
        const float channels[H723_VOFA_BNO055_CHANNEL_COUNT] = {
            g_h723_debug.bno055.acceleration_g[0],
            g_h723_debug.bno055.acceleration_g[1],
            g_h723_debug.bno055.acceleration_g[2],
            g_h723_debug.bno055.angular_rate_dps[0],
            g_h723_debug.bno055.angular_rate_dps[1],
            g_h723_debug.bno055.angular_rate_dps[2],
            g_h723_debug.bno055.angle_deg[0],
            g_h723_debug.bno055.angle_deg[1],
            g_h723_debug.bno055.angle_deg[2],
            g_h723_debug.bno055.temperature_celsius
        };
        s_last_bno055_telemetry_ms = now_ms;
        (void)vofa_justfloat_encode(s_bno055_frame, sizeof(s_bno055_frame), channels,
                                    H723_VOFA_BNO055_CHANNEL_COUNT);
        if (!h723_uart8_try_transmit(s_bno055_frame, sizeof(s_bno055_frame), now_ms)) {
            return;
        }
    }
#endif

#if (APP_GRAYSCALE_VOFA_TELEMETRY_ENABLE == 1U)
    if ((now_ms - s_last_grayscale_telemetry_ms) >= APP_GRAYSCALE_VOFA_TELEMETRY_INTERVAL_MS) {
        float channels[H723_VOFA_GRAYSCALE_CHANNEL_COUNT];
        uint32_t channel;
        s_last_grayscale_telemetry_ms = now_ms;
        for (channel = 0U; channel < 8U; ++channel) {
            channels[channel] = (float)g_h723_debug.grayscale.raw[channel];
            channels[8U + channel] =
                (float)g_h723_debug.grayscale.normalized[channel];
        }
        channels[16U] = (float)g_h723_debug.grayscale.digital;
        channels[17U] = (float)g_h723_debug.grayscale.black_mask;
        channels[18U] = (float)g_h723_debug.grayscale.black_count;
        channels[19U] = (float)g_h723_debug.grayscale.line_error;
        channels[20U] = (float)g_h723_debug.grayscale.line_strength;
        channels[21U] = (float)g_h723_debug.grayscale.sequence;
        (void)vofa_justfloat_encode(s_grayscale_frame, sizeof(s_grayscale_frame),
                                    channels, H723_VOFA_GRAYSCALE_CHANNEL_COUNT);
        if (!h723_uart8_try_transmit(s_grayscale_frame, sizeof(s_grayscale_frame), now_ms)) {
            return;
        }
    }
#endif

#if (APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_ENABLE == 1U)
    if ((now_ms - s_last_single_motor_telemetry_ms) >= APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_INTERVAL_MS) {
        bool started;

        s_last_single_motor_telemetry_ms = now_ms;
        if (g_h723_debug.single_motor.control_mode == APP_SINGLE_MOTOR_CONTROL_MODE_POSITION) {
            const float channels[H723_VOFA_SINGLE_MOTOR_POSITION_CHANNEL_COUNT] = {
                g_h723_debug.single_motor.target_position_deg,
                g_h723_debug.single_motor.feedback_position_deg,
                g_h723_debug.single_motor.position_p_out_rpm,
                g_h723_debug.single_motor.position_i_out_rpm,
                g_h723_debug.single_motor.position_d_out_rpm,
                g_h723_debug.single_motor.position_target_output_speed_rpm,
                g_h723_debug.single_motor.feedback_output_speed_rpm,
                g_h723_debug.single_motor.target_current_A,
                g_h723_debug.single_motor.feedback_current_A
            };
            (void)vofa_justfloat_encode(s_single_motor_frame,
                                        VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_SINGLE_MOTOR_POSITION_CHANNEL_COUNT),
                                        channels, H723_VOFA_SINGLE_MOTOR_POSITION_CHANNEL_COUNT);
            started = h723_uart8_try_transmit(
                s_single_motor_frame,
                VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_SINGLE_MOTOR_POSITION_CHANNEL_COUNT), now_ms);
        } else {
            const float channels[H723_VOFA_SINGLE_MOTOR_SPEED_CHANNEL_COUNT] = {
                g_h723_debug.single_motor.target_current_A,
                g_h723_debug.single_motor.feedback_current_A,
                g_h723_debug.single_motor.target_output_speed_rpm,
                g_h723_debug.single_motor.feedback_output_speed_rpm,
                g_h723_debug.single_motor.pid_output_A,
                g_h723_debug.single_motor.pid_p_out_A,
                g_h723_debug.single_motor.pid_i_out_A,
                g_h723_debug.single_motor.pid_d_out_A
            };
            (void)vofa_justfloat_encode(s_single_motor_frame,
                                        VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_SINGLE_MOTOR_SPEED_CHANNEL_COUNT),
                                        channels, H723_VOFA_SINGLE_MOTOR_SPEED_CHANNEL_COUNT);
            started = h723_uart8_try_transmit(
                s_single_motor_frame,
                VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_SINGLE_MOTOR_SPEED_CHANNEL_COUNT), now_ms);
        }
        if (!started) {
            return;
        }
    }
#endif

#if (APP_H723_K230_UART2_TEST_ENABLE == 1U)
    if ((now_ms - s_last_k230_telemetry_ms) >= APP_H723_K230_UART2_TEST_VOFA_INTERVAL_MS) {
        const float channels[H723_VOFA_K230_CHANNEL_COUNT] = {
            g_h723_debug.ball_vision.distance_mm,
            (float)g_h723_debug.ball_vision.valid,
            (float)g_h723_debug.ball_vision.frame_age_ms
        };
        s_last_k230_telemetry_ms = now_ms;
        (void)vofa_justfloat_encode(s_k230_frame, sizeof(s_k230_frame), channels,
                                    H723_VOFA_K230_CHANNEL_COUNT);
        if (!h723_uart8_try_transmit(s_k230_frame, sizeof(s_k230_frame), now_ms)) {
            return;
        }
    }
#endif

#if (APP_H723_CHASSIS_VOFA_TELEMETRY_ENABLE == 1U)
    if ((now_ms - s_last_chassis_telemetry_ms) >=
        APP_H723_CHASSIS_VOFA_TELEMETRY_INTERVAL_MS) {
        const float channels[H723_VOFA_CHASSIS_CHANNEL_COUNT] = {
            g_h723_debug.chassis.left_target_speed_mm_s,
            g_h723_debug.m2006[0].feedback_output_speed_rpm *
                APP_H723_OUTPUT_RPM_TO_MM_S,
            g_h723_debug.chassis.right_target_speed_mm_s,
            g_h723_debug.m2006[1].feedback_output_speed_rpm *
                APP_H723_OUTPUT_RPM_TO_MM_S,
            (float)g_h723_debug.grayscale.black_count,
        };
        s_last_chassis_telemetry_ms = now_ms;
        (void)vofa_justfloat_encode(s_chassis_frame, sizeof(s_chassis_frame), channels,
                                    H723_VOFA_CHASSIS_CHANNEL_COUNT);
        if (!h723_uart8_try_transmit(s_chassis_frame, sizeof(s_chassis_frame), now_ms)) {
            return;
        }
    }
#endif

#if (APP_H723_LINE_FOLLOW_PID_VOFA_TELEMETRY_ENABLE == 1U)
    if ((now_ms - s_last_line_follow_pid_telemetry_ms) >=
        APP_H723_LINE_FOLLOW_PID_VOFA_TELEMETRY_INTERVAL_MS) {
        const float channels[H723_VOFA_LINE_FOLLOW_PID_CHANNEL_COUNT] = {
            g_h723_debug.line_follow.line_position,
            g_h723_debug.line_follow.error,
            g_h723_debug.line_follow.p_out,
            g_h723_debug.line_follow.i_out,
            g_h723_debug.line_follow.d_out,
            g_h723_debug.line_follow.raw_output,
            g_h723_debug.line_follow.pid_output,
            g_h723_debug.line_follow.turn_correction_mm_s,
            g_h723_debug.line_follow.base_speed_mm_s,
            g_h723_debug.line_follow.left_target_speed_mm_s,
            g_h723_debug.line_follow.right_target_speed_mm_s,
            (float)g_h723_debug.line_follow.line_strength,
            (float)g_h723_debug.line_follow.sequence,
        };
        s_last_line_follow_pid_telemetry_ms = now_ms;
        (void)vofa_justfloat_encode(
            s_line_follow_pid_frame, sizeof(s_line_follow_pid_frame), channels,
            H723_VOFA_LINE_FOLLOW_PID_CHANNEL_COUNT);
        if (!h723_uart8_try_transmit(s_line_follow_pid_frame,
                                     sizeof(s_line_follow_pid_frame), now_ms)) {
            return;
        }
    }
#endif

#if (APP_H723_BUTTON_VOFA_TELEMETRY_ENABLE == 1U)
    if ((now_ms - s_last_button_telemetry_ms) >= APP_H723_BUTTON_VOFA_TELEMETRY_INTERVAL_MS) {
        h723_app_buttons_snapshot_t snapshot;
        float channels[H723_VOFA_BUTTON_CHANNEL_COUNT];

        s_last_button_telemetry_ms = now_ms;
        h723_app_buttons_snapshot_copy(&snapshot);
        channels[0U] = ((snapshot.stable_high_mask &
                        (1U << H723_APP_BUTTON_PC5)) != 0U) ? 1.0f : 0.0f;
        channels[1U] = ((snapshot.stable_high_mask &
                        (1U << H723_APP_BUTTON_PC4)) != 0U) ? 1.0f : 0.0f;
        channels[2U] = ((snapshot.stable_high_mask &
                        (1U << H723_APP_BUTTON_PA6)) != 0U) ? 1.0f : 0.0f;
        (void)vofa_justfloat_encode(s_button_frame, sizeof(s_button_frame),
                                    channels, H723_VOFA_BUTTON_CHANNEL_COUNT);
        if (!h723_uart8_try_transmit(s_button_frame, sizeof(s_button_frame), now_ms)) {
            return;
        }
    }
#endif

#if (APP_H723_TILT_CONTROL_VOFA_TELEMETRY_ENABLE == 1U)
    if ((now_ms - s_last_tilt_control_telemetry_ms) >=
        APP_H723_TILT_CONTROL_VOFA_TELEMETRY_INTERVAL_MS) {
        const float channels[H723_VOFA_TILT_CONTROL_CHANNEL_COUNT] = {
            g_h723_debug.tilt.target_tilt_deg,
            g_h723_debug.tilt.tilt_deg,
            g_h723_debug.tilt.error_deg,
            g_h723_debug.tilt.pid_p_out_deg_s,
            g_h723_debug.tilt.pid_i_out_deg_s,
            g_h723_debug.tilt.pid_d_out_deg_s,
            g_h723_debug.tilt.pid_rate_deg_s,
            g_h723_debug.tilt.motor_target_position_deg,
            g_h723_debug.balance.feedback_position_deg,
            g_h723_debug.balance.active_target_position_deg,
            g_h723_debug.balance.target_output_speed_rpm,
            g_h723_debug.balance.commanded_current_a,
            (float)g_h723_debug.tilt.imu_sample_age_ms,
        };
        s_last_tilt_control_telemetry_ms = now_ms;
        (void)vofa_justfloat_encode(
            s_tilt_control_frame, sizeof(s_tilt_control_frame), channels,
            H723_VOFA_TILT_CONTROL_CHANNEL_COUNT);
        if (!h723_uart8_try_transmit(s_tilt_control_frame,
                                     sizeof(s_tilt_control_frame), now_ms)) {
            return;
        }
    }
#endif
}

void h723_app_telemetry_on_uart_tx_complete(UART_HandleTypeDef *huart)
{
    if (huart == &huart8) {
        app_telemetry_tx_guard_complete(&s_uart8_tx_guard);
        g_h723_debug.uart8.tx_in_flight = 0U;
        g_h723_debug.uart8.tx_complete_count++;
        g_h723_debug.uart8.last_hal_status = HAL_OK;
        h723_uart8_publish_hal_state();
    }
}

void h723_app_telemetry_on_uart_error(UART_HandleTypeDef *huart)
{
    if (huart == &huart8) {
        app_telemetry_tx_guard_complete(&s_uart8_tx_guard);
        g_h723_debug.uart8.tx_in_flight = 0U;
        g_h723_debug.uart8.tx_drop_count++;
        g_h723_debug.uart8.last_hal_status = HAL_ERROR;
        h723_uart8_publish_hal_state();
    }
}
