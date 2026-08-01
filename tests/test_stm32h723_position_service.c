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

static void queue_m2006_feedback(uint32_t now_ms, uint16_t encoder)
{
    s_now_ms = now_ms;
    s_rx_header.IdType = FDCAN_STANDARD_ID;
    s_rx_header.DataLength = FDCAN_DLC_BYTES_8;
    s_rx_header.Identifier = 0x201U;
    memset(s_rx_data, 0, sizeof(s_rx_data));
    s_rx_data[0] = (uint8_t)(encoder >> 8U);
    s_rx_data[1] = (uint8_t)encoder;
    s_rx_pending = 1U;
    h723_chassis_on_fdcan2_rx();
    h723_chassis_service_step(now_ms);
}

static void test_position_feedback_survives_valid_slow_feedback(void)
{
    h723_chassis_service_init();
    g_h723_debug.single_motor.enable = 1U;
    g_h723_debug.single_motor.control_mode = 1U;
    g_h723_debug.single_motor.selected_id = 1U;
    g_h723_debug.single_motor.target_position_deg = 10.0f;

    queue_m2006_feedback(0U, 1000U);
    queue_m2006_feedback(10U, 2000U);
    queue_m2006_feedback(20U, 3000U);

    assert(g_h723_debug.fdcan.rx_count == 3U);
    assert(g_h723_debug.m2006[0].feedback_age_ms == 0U);
    assert(g_h723_debug.single_motor.position_reference_valid == 1U);
    assert(g_h723_debug.single_motor.feedback_encoder == 3000U);
    assert(fabsf(g_h723_debug.single_motor.feedback_position_deg) > 0.1f);

    queue_m2006_feedback(80U, 4000U);
    assert(g_h723_debug.single_motor.position_reference_valid == 1U);
    assert(g_h723_debug.single_motor.feedback_position_deg == 0.0f);
}

int main(void)
{
    test_position_feedback_survives_valid_slow_feedback();
    return 0;
}
