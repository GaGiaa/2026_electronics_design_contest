#include "app_bno055_service.h"

#include <limits.h>

#include "cmsis_os.h"

#include "app_bno055.h"
#include "app_config.h"
#include "app_debug.h"
#include "usart.h"

static app_bno055_t s_imu;
static uint32_t s_last_sample_ms;
static uint32_t s_next_start_attempt_ms;
static uint32_t s_start_attempt_count;
static uint32_t s_uart_error_count;
static app_bno055_snapshot_t s_control_snapshot;
static volatile uint32_t s_control_snapshot_sequence;
static volatile uint32_t s_control_snapshot_age_ms;

#define H723_BNO055_DRAIN_MAX_BYTES 16U

static int h723_bno055_write(void *context, const uint8_t *data, size_t length, uint32_t timeout_ms)
{
    HAL_StatusTypeDef status;

    (void)context;
    status = HAL_UART_Transmit(&huart1, (uint8_t *)data, (uint16_t)length, timeout_ms);
    if (status != HAL_OK) {
        s_uart_error_count++;
        app_bno055_set_transport_diagnostics(&s_imu, (int)status, huart1.ErrorCode);
        return 0;
    }
    return 1;
}

static int h723_bno055_read(void *context, uint8_t *data, size_t length, uint32_t timeout_ms)
{
    HAL_StatusTypeDef status;

    (void)context;
    status = HAL_UART_Receive(&huart1, data, (uint16_t)length, timeout_ms);
    if (status != HAL_OK) {
        s_uart_error_count++;
        app_bno055_set_transport_diagnostics(&s_imu, (int)status, huart1.ErrorCode);
        return 0;
    }
    return 1;
}

static int h723_bno055_drain(void *context, uint32_t timeout_ms)
{
    uint32_t drained = 0U;

    (void)context;
    (void)timeout_ms;
    (void)HAL_UART_Abort(&huart1);
    __HAL_UART_CLEAR_PEFLAG(&huart1);
    __HAL_UART_CLEAR_FEFLAG(&huart1);
    __HAL_UART_CLEAR_NEFLAG(&huart1);
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    while ((__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET) &&
           (drained < H723_BNO055_DRAIN_MAX_BYTES)) {
        volatile uint32_t discard = huart1.Instance->RDR;
        (void)discard;
        drained++;
    }
    __HAL_UART_CLEAR_PEFLAG(&huart1);
    __HAL_UART_CLEAR_FEFLAG(&huart1);
    __HAL_UART_CLEAR_NEFLAG(&huart1);
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    return 1;
}

static void h723_bno055_delay(void *context, uint32_t delay_ms)
{
    (void)context;
    (void)osDelay(delay_ms);
}

static void h723_bno055_publish_debug(uint32_t now_ms)
{
    const app_bno055_snapshot_t *snapshot = app_bno055_get_snapshot(&s_imu);
    uint32_t index;

    if (snapshot == NULL) {
        return;
    }
    for (index = 0U; index < 3U; ++index) {
        g_h723_debug.bno055.acceleration_raw[index] = snapshot->acceleration_raw[index];
        g_h723_debug.bno055.angular_rate_raw[index] = snapshot->angular_rate_raw[index];
        g_h723_debug.bno055.angle_raw[index] = snapshot->angle_raw[index];
        g_h723_debug.bno055.acceleration_g[index] = snapshot->acceleration_g[index];
        g_h723_debug.bno055.angular_rate_dps[index] = snapshot->angular_rate_dps[index];
    }
    g_h723_debug.bno055.angle_deg[0] = snapshot->roll_deg;
    g_h723_debug.bno055.angle_deg[1] = snapshot->pitch_deg;
    g_h723_debug.bno055.angle_deg[2] = snapshot->yaw_deg;
    g_h723_debug.bno055.temperature_raw = snapshot->temperature_raw;
    g_h723_debug.bno055.temperature_celsius = snapshot->temperature_celsius;
    g_h723_debug.bno055.calibration_sys = snapshot->calibration.sys;
    g_h723_debug.bno055.calibration_gyro = snapshot->calibration.gyro;
    g_h723_debug.bno055.calibration_acc = snapshot->calibration.acc;
    g_h723_debug.bno055.calibration_mag = snapshot->calibration.mag;
    g_h723_debug.bno055.sys_status = snapshot->sys_status;
    g_h723_debug.bno055.sys_err = snapshot->sys_err;
    g_h723_debug.bno055.complete_sample_count = snapshot->complete_sample_count;
    g_h723_debug.bno055.start_attempt_count = s_start_attempt_count;
    g_h723_debug.bno055.uart_error_count = s_uart_error_count;
    g_h723_debug.bno055.last_error = snapshot->last_error;
    g_h723_debug.bno055.last_stage = snapshot->last_stage;
    g_h723_debug.bno055.last_detail = snapshot->last_detail;
    g_h723_debug.bno055.transport_status = snapshot->transport_status;
    g_h723_debug.bno055.hal_error_code = snapshot->hal_error_code;
    g_h723_debug.bno055.last_rx0 = snapshot->last_rx0;
    g_h723_debug.bno055.last_rx1 = snapshot->last_rx1;
    g_h723_debug.bno055.online = snapshot->online != 0 ? 1U : 0U;
    g_h723_debug.bno055.sample_valid = snapshot->valid != 0 ? 1U : 0U;
    g_h723_debug.bno055.sample_age_ms = snapshot->valid != 0 ?
        (uint32_t)(now_ms - s_last_sample_ms) : UINT_MAX;
}

static void h723_bno055_publish_control_snapshot(uint32_t now_ms)
{
    const app_bno055_snapshot_t *snapshot = app_bno055_get_snapshot(&s_imu);

    if (snapshot == NULL) {
        return;
    }

    s_control_snapshot_sequence++;
    __DMB();
    s_control_snapshot = *snapshot;
    s_control_snapshot_age_ms = snapshot->valid != 0 ?
        (uint32_t)(now_ms - s_last_sample_ms) : UINT_MAX;
    __DMB();
    s_control_snapshot_sequence++;
}

bool h723_bno055_service_get_snapshot(app_bno055_snapshot_t *snapshot,
                                      uint32_t *sample_age_ms)
{
    uint32_t sequence_before;
    uint32_t sequence_after;

    if (snapshot == NULL || sample_age_ms == NULL) {
        return false;
    }

    sequence_before = s_control_snapshot_sequence;
    if ((sequence_before & 1U) != 0U) {
        return false;
    }
    __DMB();
    *snapshot = s_control_snapshot;
    *sample_age_ms = s_control_snapshot_age_ms;
    __DMB();
    sequence_after = s_control_snapshot_sequence;
    return sequence_before == sequence_after && (sequence_after & 1U) == 0U;
}

void h723_bno055_service_init(void)
{
    app_bno055_init(&s_imu, NULL, h723_bno055_write, h723_bno055_read,
                    h723_bno055_delay, APP_BNO055_UART_TIMEOUT_MS);
    app_bno055_set_drain(&s_imu, h723_bno055_drain);
    s_control_snapshot_sequence = 0U;
    s_control_snapshot_age_ms = UINT_MAX;
}

void h723_bno055_service_step(uint32_t now_ms)
{
    int result;

    if (!s_imu.initialized) {
        if ((int32_t)(now_ms - s_next_start_attempt_ms) >= 0) {
            app_bno055_reset(&s_imu);
            s_start_attempt_count++;
            result = app_bno055_start(&s_imu);
            if (result != APP_BNO055_OK) {
                s_next_start_attempt_ms = now_ms + APP_BNO055_RETRY_INTERVAL_MS;
            }
        }
        h723_bno055_publish_debug(now_ms);
        h723_bno055_publish_control_snapshot(now_ms);
        return;
    }

    result = app_bno055_poll_step(&s_imu);
    if (result == APP_BNO055_OK) {
        s_last_sample_ms = now_ms;
    } else if (result < APP_BNO055_OK) {
        s_next_start_attempt_ms = now_ms + APP_BNO055_RETRY_INTERVAL_MS;
    }
    h723_bno055_publish_debug(now_ms);
    h723_bno055_publish_control_snapshot(now_ms);
}
