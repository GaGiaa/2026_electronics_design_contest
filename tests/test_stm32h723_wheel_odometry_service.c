#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "app_chassis_service.h"
#include "app_debug.h"
#include "app_time.h"
#include "fdcan.h"
#include "usart.h"

FDCAN_HandleTypeDef hfdcan1 = {1U};
FDCAN_HandleTypeDef hfdcan2 = {2U};
FDCAN_HandleTypeDef hfdcan3 = {3U};
UART_HandleTypeDef huart7 = {0};
UART_HandleTypeDef huart8 = {0};
UART_HandleTypeDef huart9 = {0};
volatile h723_debug_t g_h723_debug = {0};

static uint32_t s_now_ms;
static uint32_t s_rx_pending;
static FDCAN_RxHeaderTypeDef s_rx_header;
static uint8_t s_rx_data[8];

uint32_t h723_app_time_now_ms(void)
{
    return s_now_ms;
}

HAL_StatusTypeDef HAL_FDCAN_GetProtocolStatus(FDCAN_HandleTypeDef *fdcan,
                                                FDCAN_ProtocolStatusTypeDef *status)
{
    (void)fdcan;
    memset(status, 0, sizeof(*status));
    return HAL_OK;
}

HAL_StatusTypeDef HAL_FDCAN_GetErrorCounters(FDCAN_HandleTypeDef *fdcan,
                                               FDCAN_ErrorCountersTypeDef *counters)
{
    (void)fdcan;
    memset(counters, 0, sizeof(*counters));
    return HAL_OK;
}

HAL_StatusTypeDef HAL_FDCAN_ConfigFilter(FDCAN_HandleTypeDef *fdcan,
                                          const FDCAN_FilterTypeDef *filter)
{
    (void)fdcan;
    (void)filter;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_FDCAN_Start(FDCAN_HandleTypeDef *fdcan)
{
    (void)fdcan;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_FDCAN_ActivateNotification(FDCAN_HandleTypeDef *fdcan,
                                                   uint32_t active_it,
                                                   uint32_t buffer)
{
    (void)fdcan;
    (void)active_it;
    (void)buffer;
    return HAL_OK;
}

uint32_t HAL_FDCAN_GetRxFifoFillLevel(const FDCAN_HandleTypeDef *fdcan,
                                      uint32_t rx_fifo)
{
    (void)rx_fifo;
    return (fdcan == &hfdcan2) ? s_rx_pending : 0U;
}

HAL_StatusTypeDef HAL_FDCAN_GetRxMessage(FDCAN_HandleTypeDef *fdcan,
                                         uint32_t rx_fifo,
                                         FDCAN_RxHeaderTypeDef *header,
                                         uint8_t data[8])
{
    (void)fdcan;
    (void)rx_fifo;
    *header = s_rx_header;
    memcpy(data, s_rx_data, sizeof(s_rx_data));
    s_rx_pending = 0U;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_FDCAN_AddMessageToTxFifoQ(FDCAN_HandleTypeDef *fdcan,
                                                 const FDCAN_TxHeaderTypeDef *header,
                                                 const uint8_t data[8])
{
    (void)fdcan;
    (void)header;
    (void)data;
    return HAL_OK;
}

int HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *huart,
                                 uint8_t *buffer,
                                 uint16_t size)
{
    (void)huart;
    (void)buffer;
    (void)size;
    return HAL_UART_TEST_OK;
}

static void queue_m2006_feedback(uint32_t now_ms, uint32_t identifier,
                                 uint16_t encoder)
{
    s_now_ms = now_ms;
    s_rx_header.IdType = FDCAN_STANDARD_ID;
    s_rx_header.DataLength = FDCAN_DLC_BYTES_8;
    s_rx_header.Identifier = identifier;
    memset(s_rx_data, 0, sizeof(s_rx_data));
    s_rx_data[0] = (uint8_t)(encoder >> 8U);
    s_rx_data[1] = (uint8_t)encoder;
    s_rx_pending = 1U;
    h723_chassis_on_fdcan2_rx();
    h723_chassis_service_step(now_ms);
}

static void test_two_wheel_encoder_odometry_and_wrap(void)
{
    h723_chassis_service_init();

    queue_m2006_feedback(0U, 0x201U, 8190U);
    queue_m2006_feedback(0U, 0x202U, 100U);
    queue_m2006_feedback(10U, 0x201U, 2U);
    queue_m2006_feedback(10U, 0x202U, 96U);

    assert(g_h723_debug.wheel_odometry.valid == 1U);
    assert(g_h723_debug.wheel_odometry.left_encoder == 2U);
    assert(g_h723_debug.wheel_odometry.right_encoder == 96U);
    assert(g_h723_debug.wheel_odometry.left_motor_counts == 4);
    assert(g_h723_debug.wheel_odometry.right_motor_counts == -4);
    assert(g_h723_debug.wheel_odometry.x_mm > 0.0f);
    assert(fabsf(g_h723_debug.wheel_odometry.y_mm) < 0.001f);
    assert(fabsf(g_h723_debug.wheel_odometry.yaw_deg) < 0.001f);
}

static void test_timeout_recovery_and_watch_reset(void)
{
    const float distance_before_timeout = g_h723_debug.wheel_odometry.x_mm;

    h723_chassis_service_step(60U);
    assert(g_h723_debug.wheel_odometry.valid == 0U);
    assert(fabsf(g_h723_debug.wheel_odometry.x_mm - distance_before_timeout) < 0.001f);

    queue_m2006_feedback(70U, 0x201U, 4U);
    queue_m2006_feedback(70U, 0x202U, 94U);
    assert(g_h723_debug.wheel_odometry.valid == 1U);
    assert(g_h723_debug.wheel_odometry.rebaseline_count == 1U);
    assert(fabsf(g_h723_debug.wheel_odometry.delta_distance_mm) < 0.001f);
    assert(fabsf(g_h723_debug.wheel_odometry.x_mm - distance_before_timeout) < 0.001f);

    g_h723_debug.wheel_odometry.reset_request = 1U;
    h723_chassis_service_step(80U);
    assert(g_h723_debug.wheel_odometry.reset_count == 1U);
    assert(fabsf(g_h723_debug.wheel_odometry.x_mm) < 0.001f);
    assert(fabsf(g_h723_debug.wheel_odometry.y_mm) < 0.001f);
    assert(fabsf(g_h723_debug.wheel_odometry.yaw_deg) < 0.001f);
    assert(g_h723_debug.wheel_odometry.reset_request == 0U);
}

static uint16_t reverse_encoder(uint16_t start_encoder, uint32_t motor_counts)
{
    const int32_t modulus = 8192;
    int32_t value = ((int32_t)start_encoder - (int32_t)(motor_counts % (uint32_t)modulus)) % modulus;

    if (value < 0) {
        value += modulus;
    }
    return (uint16_t)value;
}

static void test_service_full_rotation_wraps_heading_after_multiple_encoder_wraps(void)
{
    const uint32_t step_counts = 3000U;
    const uint32_t step_count = 310U;
    const uint32_t total_counts = step_counts * step_count;
    uint32_t index;

    h723_chassis_service_init();
    queue_m2006_feedback(0U, 0x201U, 2U);
    queue_m2006_feedback(0U, 0x202U, 96U);
    for (index = 1U; index <= step_count; ++index) {
        const uint32_t motor_counts = step_counts * index;
        const uint32_t now_ms = index;

        queue_m2006_feedback(now_ms, 0x201U, reverse_encoder(2U, motor_counts));
        queue_m2006_feedback(now_ms, 0x202U, reverse_encoder(96U, motor_counts));
    }

    assert(g_h723_debug.wheel_odometry.valid == 1U);
    assert(fabsf(g_h723_debug.wheel_odometry.x_mm) < 0.1f);
    assert(fabsf(g_h723_debug.wheel_odometry.y_mm) < 0.1f);
    assert(fabsf(g_h723_debug.wheel_odometry.yaw_deg) < 0.1f);
    assert(fabsf(g_h723_debug.wheel_odometry.yaw_deg_continuous -
                 ((float)total_counts * 2.0f * 65.0f * 180.0f) /
                 ((float)(8192U * 36U) * 205.0f)) < 0.1f);
}

int main(void)
{
    test_two_wheel_encoder_odometry_and_wrap();
    test_timeout_recovery_and_watch_reset();
    test_service_full_rotation_wraps_heading_after_multiple_encoder_wraps();
    return 0;
}
