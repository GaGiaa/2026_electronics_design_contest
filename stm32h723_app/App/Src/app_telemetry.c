#include "app_telemetry.h"

#include "app_config.h"
#include "app_debug.h"
#include "app_time.h"
#include "usart.h"
#include "vofa_justfloat.h"

#define H723_VOFA_HEALTH_CHANNEL_COUNT 6U
#define H723_VOFA_HEALTH_MAGIC 723.0f
#define H723_VOFA_JY901S_CHANNEL_COUNT 10U
#define H723_VOFA_SINGLE_MOTOR_CHANNEL_COUNT 8U

#if (APP_VOFA_HEALTH_TELEMETRY_ENABLE == 1U)
static uint8_t s_health_frame[VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_HEALTH_CHANNEL_COUNT)];
static uint32_t s_last_telemetry_ms;
#endif

#if (APP_JY901S_VOFA_TELEMETRY_ENABLE == 1U)
static uint8_t s_jy901s_frame[VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_JY901S_CHANNEL_COUNT)];
static uint32_t s_last_jy901s_telemetry_ms;
#endif

#if (APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_ENABLE == 1U)
static uint8_t s_single_motor_frame[VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_SINGLE_MOTOR_CHANNEL_COUNT)];
static uint32_t s_last_single_motor_telemetry_ms;
#endif

void h723_app_telemetry_init(void)
{
    g_h723_debug.system.boot_count++;
    g_h723_debug.uart8.telemetry_enabled = APP_VOFA_HEALTH_TELEMETRY_ENABLE |
                                            APP_JY901S_VOFA_TELEMETRY_ENABLE |
                                            (APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_ENABLE << 2U);
    g_h723_debug.uart8.last_hal_status = HAL_OK;
#if (APP_VOFA_HEALTH_TELEMETRY_ENABLE == 1U)
    s_last_telemetry_ms = h723_app_time_now_ms();
#endif
#if (APP_JY901S_VOFA_TELEMETRY_ENABLE == 1U)
    s_last_jy901s_telemetry_ms = h723_app_time_now_ms();
#endif
#if (APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_ENABLE == 1U)
    s_last_single_motor_telemetry_ms = h723_app_time_now_ms();
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

#if (APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_ENABLE == 1U)
    if ((now_ms - s_last_single_motor_telemetry_ms) >= APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_INTERVAL_MS) {
        const float channels[H723_VOFA_SINGLE_MOTOR_CHANNEL_COUNT] = {
            g_h723_debug.single_motor.target_current_A,
            g_h723_debug.single_motor.feedback_current_A,
            g_h723_debug.single_motor.target_output_speed_rpm,
            g_h723_debug.single_motor.feedback_output_speed_rpm,
            g_h723_debug.single_motor.pid_output_A,
            g_h723_debug.single_motor.pid_p_out_A,
            g_h723_debug.single_motor.pid_i_out_A,
            g_h723_debug.single_motor.pid_d_out_A
        };
        HAL_StatusTypeDef status;

        s_last_single_motor_telemetry_ms = now_ms;
        if (g_h723_debug.uart8.tx_in_flight != 0U) {
            g_h723_debug.uart8.tx_drop_count++;
            return;
        }
        (void)vofa_justfloat_encode(s_single_motor_frame, sizeof(s_single_motor_frame), channels,
                                    H723_VOFA_SINGLE_MOTOR_CHANNEL_COUNT);
        status = HAL_UART_Transmit_DMA(&huart8, s_single_motor_frame, sizeof(s_single_motor_frame));
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
