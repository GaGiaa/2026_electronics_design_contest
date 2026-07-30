#include "app_telemetry.h"

#include "app_config.h"
#include "app_debug.h"
#include "app_single_motor.h"
#include "app_time.h"
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

void h723_app_telemetry_init(void)
{
    g_h723_debug.system.boot_count++;
    g_h723_debug.uart8.telemetry_enabled = APP_VOFA_HEALTH_TELEMETRY_ENABLE |
        APP_JY901S_VOFA_TELEMETRY_ENABLE |
        (APP_GRAYSCALE_VOFA_TELEMETRY_ENABLE << 1U) |
        (APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_ENABLE << 2U) |
        (APP_H723_K230_UART2_TEST_ENABLE << 3U);
    g_h723_debug.uart8.last_hal_status = HAL_OK;
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
}

void h723_app_telemetry_step(void)
{
    uint32_t now_ms = h723_app_time_now_ms();

    g_h723_debug.system.uptime_ms = now_ms;
    g_h723_debug.system.task_loop_count++;

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
        HAL_StatusTypeDef status;

        s_last_telemetry_ms = now_ms;
        if (g_h723_debug.uart8.tx_in_flight != 0U) {
            g_h723_debug.uart8.tx_drop_count++;
            return;
        }

        (void)vofa_justfloat_encode(s_health_frame, sizeof(s_health_frame), channels,
                                    H723_VOFA_HEALTH_CHANNEL_COUNT);
        status = HAL_UART_Transmit_DMA(&huart8, s_health_frame, sizeof(s_health_frame));
        g_h723_debug.uart8.last_hal_status = (uint32_t)status;
        if (status == HAL_OK) {
            g_h723_debug.uart8.tx_in_flight = 1U;
            g_h723_debug.uart8.tx_start_count++;
        } else {
            g_h723_debug.uart8.tx_drop_count++;
        }
    }
#endif

#if (APP_JY901S_VOFA_TELEMETRY_ENABLE == 1U)
    if (g_h723_debug.jy901s.sample_valid != 0U &&
        (now_ms - s_last_jy901s_telemetry_ms) >= APP_JY901S_VOFA_TELEMETRY_INTERVAL_MS) {
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
        HAL_StatusTypeDef status;

        s_last_jy901s_telemetry_ms = now_ms;
        if (g_h723_debug.uart8.tx_in_flight != 0U) {
            g_h723_debug.uart8.tx_drop_count++;
            return;
        }
        (void)vofa_justfloat_encode(s_jy901s_frame, sizeof(s_jy901s_frame), channels,
                                    H723_VOFA_JY901S_CHANNEL_COUNT);
        status = HAL_UART_Transmit_DMA(&huart8, s_jy901s_frame, sizeof(s_jy901s_frame));
        g_h723_debug.uart8.last_hal_status = (uint32_t)status;
        if (status == HAL_OK) {
            g_h723_debug.uart8.tx_in_flight = 1U;
            g_h723_debug.uart8.tx_start_count++;
        } else {
            g_h723_debug.uart8.tx_drop_count++;
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
        HAL_StatusTypeDef status;

        s_last_bno055_telemetry_ms = now_ms;
        if (g_h723_debug.uart8.tx_in_flight != 0U) {
            g_h723_debug.uart8.tx_drop_count++;
            return;
        }
        (void)vofa_justfloat_encode(s_bno055_frame, sizeof(s_bno055_frame), channels,
                                    H723_VOFA_BNO055_CHANNEL_COUNT);
        status = HAL_UART_Transmit_DMA(&huart8, s_bno055_frame, sizeof(s_bno055_frame));
        g_h723_debug.uart8.last_hal_status = (uint32_t)status;
        if (status == HAL_OK) {
            g_h723_debug.uart8.tx_in_flight = 1U;
            g_h723_debug.uart8.tx_start_count++;
        } else {
            g_h723_debug.uart8.tx_drop_count++;
        }
    }
#endif

#if (APP_GRAYSCALE_VOFA_TELEMETRY_ENABLE == 1U)
    if ((now_ms - s_last_grayscale_telemetry_ms) >= APP_GRAYSCALE_VOFA_TELEMETRY_INTERVAL_MS) {
        float channels[H723_VOFA_GRAYSCALE_CHANNEL_COUNT];
        uint32_t channel;
        HAL_StatusTypeDef status;

        s_last_grayscale_telemetry_ms = now_ms;
        if (g_h723_debug.uart8.tx_in_flight != 0U) {
            g_h723_debug.uart8.tx_drop_count++;
            return;
        }
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
        status = HAL_UART_Transmit_DMA(&huart8, s_grayscale_frame,
                                       sizeof(s_grayscale_frame));
        g_h723_debug.uart8.last_hal_status = (uint32_t)status;
        if (status == HAL_OK) {
            g_h723_debug.uart8.tx_in_flight = 1U;
            g_h723_debug.uart8.tx_start_count++;
        } else {
            g_h723_debug.uart8.tx_drop_count++;
        }
    }
#endif

#if (APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_ENABLE == 1U)
    if ((now_ms - s_last_single_motor_telemetry_ms) >= APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_INTERVAL_MS) {
        HAL_StatusTypeDef status;

        s_last_single_motor_telemetry_ms = now_ms;
        if (g_h723_debug.uart8.tx_in_flight != 0U) {
            g_h723_debug.uart8.tx_drop_count++;
            return;
        }
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
            status = HAL_UART_Transmit_DMA(&huart8, s_single_motor_frame,
                                            VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_SINGLE_MOTOR_POSITION_CHANNEL_COUNT));
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
            status = HAL_UART_Transmit_DMA(&huart8, s_single_motor_frame,
                                            VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_SINGLE_MOTOR_SPEED_CHANNEL_COUNT));
        }
        g_h723_debug.uart8.last_hal_status = (uint32_t)status;
        if (status == HAL_OK) {
            g_h723_debug.uart8.tx_in_flight = 1U;
            g_h723_debug.uart8.tx_start_count++;
        } else {
            g_h723_debug.uart8.tx_drop_count++;
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
        HAL_StatusTypeDef status;

        s_last_k230_telemetry_ms = now_ms;
        if (g_h723_debug.uart8.tx_in_flight != 0U) {
            g_h723_debug.uart8.tx_drop_count++;
            return;
        }
        (void)vofa_justfloat_encode(s_k230_frame, sizeof(s_k230_frame), channels,
                                    H723_VOFA_K230_CHANNEL_COUNT);
        status = HAL_UART_Transmit_DMA(&huart8, s_k230_frame, sizeof(s_k230_frame));
        g_h723_debug.uart8.last_hal_status = (uint32_t)status;
        if (status == HAL_OK) {
            g_h723_debug.uart8.tx_in_flight = 1U;
            g_h723_debug.uart8.tx_start_count++;
        } else {
            g_h723_debug.uart8.tx_drop_count++;
        }
    }
#endif
}

void h723_app_telemetry_on_uart_tx_complete(UART_HandleTypeDef *huart)
{
    if (huart == &huart8) {
        g_h723_debug.uart8.tx_in_flight = 0U;
        g_h723_debug.uart8.tx_complete_count++;
        g_h723_debug.uart8.last_hal_status = HAL_OK;
    }
}

void h723_app_telemetry_on_uart_error(UART_HandleTypeDef *huart)
{
    if (huart == &huart8) {
        g_h723_debug.uart8.tx_in_flight = 0U;
        g_h723_debug.uart8.tx_drop_count++;
        g_h723_debug.uart8.last_hal_status = HAL_ERROR;
    }
}
