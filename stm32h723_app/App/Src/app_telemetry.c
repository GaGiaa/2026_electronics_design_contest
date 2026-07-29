#include "app_telemetry.h"

#include "app_config.h"
#include "app_debug.h"
#include "usart.h"
#include "vofa_justfloat.h"

#define H723_VOFA_HEALTH_CHANNEL_COUNT 6U
#define H723_VOFA_HEALTH_MAGIC 723.0f

#if (APP_VOFA_HEALTH_TELEMETRY_ENABLE == 1U)
static uint8_t s_health_frame[VOFA_JUSTFLOAT_FRAME_SIZE(H723_VOFA_HEALTH_CHANNEL_COUNT)];
static uint32_t s_last_telemetry_ms;
#endif

void h723_app_telemetry_init(void)
{
    g_h723_debug.boot_count++;
    g_h723_debug.telemetry_enabled = APP_VOFA_HEALTH_TELEMETRY_ENABLE;
    g_h723_debug.last_hal_status = HAL_OK;
#if (APP_VOFA_HEALTH_TELEMETRY_ENABLE == 1U)
    s_last_telemetry_ms = HAL_GetTick();
#endif
}

void h723_app_telemetry_step(void)
{
    uint32_t now_ms = HAL_GetTick();

    g_h723_debug.uptime_ms = now_ms;
    g_h723_debug.task_loop_count++;

#if (APP_VOFA_HEALTH_TELEMETRY_ENABLE == 1U)
    if ((now_ms - s_last_telemetry_ms) >= APP_VOFA_HEALTH_TELEMETRY_INTERVAL_MS) {
        const float channels[H723_VOFA_HEALTH_CHANNEL_COUNT] = {
            H723_VOFA_HEALTH_MAGIC,
            (float)g_h723_debug.uptime_ms,
            (float)g_h723_debug.task_loop_count,
            (float)g_h723_debug.tx_start_count,
            (float)g_h723_debug.tx_complete_count,
            (float)g_h723_debug.tx_drop_count
        };
        HAL_StatusTypeDef status;

        s_last_telemetry_ms = now_ms;
        if (g_h723_debug.tx_in_flight != 0U) {
            g_h723_debug.tx_drop_count++;
            return;
        }

        (void)vofa_justfloat_encode(s_health_frame, sizeof(s_health_frame), channels,
                                    H723_VOFA_HEALTH_CHANNEL_COUNT);
        status = HAL_UART_Transmit_DMA(&huart8, s_health_frame, sizeof(s_health_frame));
        g_h723_debug.last_hal_status = (uint32_t)status;
        if (status == HAL_OK) {
            g_h723_debug.tx_in_flight = 1U;
            g_h723_debug.tx_start_count++;
        } else {
            g_h723_debug.tx_drop_count++;
        }
    }
#endif
}

void h723_app_telemetry_on_uart_tx_complete(UART_HandleTypeDef *huart)
{
    if (huart == &huart8) {
        g_h723_debug.tx_in_flight = 0U;
        g_h723_debug.tx_complete_count++;
        g_h723_debug.last_hal_status = HAL_OK;
    }
}

void h723_app_telemetry_on_uart_error(UART_HandleTypeDef *huart)
{
    if (huart == &huart8) {
        g_h723_debug.tx_in_flight = 0U;
        g_h723_debug.tx_drop_count++;
        g_h723_debug.last_hal_status = HAL_ERROR;
    }
}
