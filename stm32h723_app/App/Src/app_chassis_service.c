#include "app_chassis_service.h"

#include <string.h>

#include "app_chassis.h"
#include "app_config.h"
#include "app_crsf.h"
#include "app_debug.h"
#include "app_m2006.h"
#include "app_time.h"
#include "fdcan.h"
#include "usart.h"

#define H723_CRSF_DMA_BUFFER_SIZE 64U
#define H723_CRSF_RING_SIZE 256U

static uint8_t s_crsf_dma_buffer[H723_CRSF_DMA_BUFFER_SIZE];
static volatile uint8_t s_crsf_ring[H723_CRSF_RING_SIZE];
static volatile uint16_t s_crsf_write_index;
static uint16_t s_crsf_read_index;
static app_crsf_parser_t s_crsf_parser;
static app_crsf_input_t s_crsf_input;
static app_chassis_command_t s_command;
static app_m2006_feedback_t s_feedback[2];
static uint32_t s_feedback_time_ms[2];
static bool s_feedback_valid[2];
static PID_Incremental s_speed_pid[2];
static uint32_t s_crsf_uart_error_count;
static uint32_t s_crsf_ring_overrun_count;
static bool s_crsf_was_timed_out;

static void h723_chassis_on_fdcan_rx(FDCAN_HandleTypeDef *fdcan);

static FDCAN_HandleTypeDef *h723_m2006_fdcan(void)
{
#if APP_H723_M2006_FDCAN_INSTANCE == 1U
    return &hfdcan1;
#elif APP_H723_M2006_FDCAN_INSTANCE == 2U
    return &hfdcan2;
#elif APP_H723_M2006_FDCAN_INSTANCE == 3U
    return &hfdcan3;
#else
#error "APP_H723_M2006_FDCAN_INSTANCE must be 1U, 2U, or 3U"
#endif
}

static void h723_update_fdcan_diagnostics(void)
{
    FDCAN_ProtocolStatusTypeDef protocol = {0};
    FDCAN_ErrorCountersTypeDef counters = {0};
    FDCAN_HandleTypeDef *fdcan = h723_m2006_fdcan();

    g_h723_debug.fdcan.instance = APP_H723_M2006_FDCAN_INSTANCE;
    if (HAL_FDCAN_GetProtocolStatus(fdcan, &protocol) == HAL_OK) {
        g_h723_debug.fdcan.protocol_last_error = protocol.LastErrorCode;
        g_h723_debug.fdcan.protocol_activity = protocol.Activity;
        g_h723_debug.fdcan.protocol_bus_off = protocol.BusOff;
    }
    if (HAL_FDCAN_GetErrorCounters(fdcan, &counters) == HAL_OK) {
        g_h723_debug.fdcan.tx_error_counter = counters.TxErrorCnt;
        g_h723_debug.fdcan.rx_error_counter = counters.RxErrorCnt;
    }
}

static const PID_Incremental_Param_Config s_speed_pid_params = {
    .kp = APP_H723_M2006_PID_KP,
    .ki = APP_H723_M2006_PID_KI,
    .kd = APP_H723_M2006_PID_KD,
    .output_limit = APP_H723_M2006_CURRENT_LIMIT,
    .deadband = 10.0f,
    .integral_output_limit = APP_H723_M2006_PID_INTEGRAL_LIMIT,
    .integral_separation_threshold = 800.0f,
    .derivative_filter_N = 0.0f,
    .output_delta_limit = APP_H723_M2006_PID_OUTPUT_DELTA_LIMIT,
};

static int16_t h723_clamp_current(float value)
{
    if (value > APP_H723_M2006_CURRENT_LIMIT) { return (int16_t)APP_H723_M2006_CURRENT_LIMIT; }
    if (value < -APP_H723_M2006_CURRENT_LIMIT) { return (int16_t)-APP_H723_M2006_CURRENT_LIMIT; }
    return (int16_t)value;
}

static uint32_t h723_crsf_switch_state(uint16_t raw)
{
    if (raw <= 700U) { return 0U; }
    if (raw >= 1300U) { return 2U; }
    return 1U;
}

static void h723_start_uart7_receive(void)
{
    HAL_StatusTypeDef status = HAL_UARTEx_ReceiveToIdle_DMA(&huart7, s_crsf_dma_buffer, sizeof(s_crsf_dma_buffer));
    if (status == HAL_OK && huart7.hdmarx != NULL) { __HAL_DMA_DISABLE_IT(huart7.hdmarx, DMA_IT_HT); }
}

void h723_chassis_service_init(void)
{
    FDCAN_FilterTypeDef filter = {0};
    FDCAN_HandleTypeDef *fdcan = h723_m2006_fdcan();
    uint32_t index;
    app_crsf_parser_init(&s_crsf_parser);
    for (index = 0U; index < 2U; ++index) { PID_Incremental_Init(&s_speed_pid[index], &s_speed_pid_params, 0.001f); }
    filter.IdType = FDCAN_STANDARD_ID;
    filter.FilterIndex = 0U;
    filter.FilterType = FDCAN_FILTER_RANGE;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1 = 0x201U;
    filter.FilterID2 = 0x202U;
    if (HAL_FDCAN_ConfigFilter(fdcan, &filter) == HAL_OK &&
        HAL_FDCAN_Start(fdcan) == HAL_OK &&
        HAL_FDCAN_ActivateNotification(fdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0U) == HAL_OK) {
        g_h723_debug.fdcan.last_status = HAL_OK;
    } else { g_h723_debug.fdcan.last_status = HAL_ERROR; }
    h723_start_uart7_receive();
}

void h723_chassis_on_uart7_rx_event(uint16_t size)
{
    uint16_t index;
    if (size > H723_CRSF_DMA_BUFFER_SIZE) { size = H723_CRSF_DMA_BUFFER_SIZE; }
    for (index = 0U; index < size; ++index) {
        uint16_t next = (uint16_t)((s_crsf_write_index + 1U) % H723_CRSF_RING_SIZE);
        if (next != s_crsf_read_index) { s_crsf_ring[s_crsf_write_index] = s_crsf_dma_buffer[index]; s_crsf_write_index = next; }
        else { ++s_crsf_ring_overrun_count; }
    }
    h723_start_uart7_receive();
}

void h723_chassis_on_uart7_error(void)
{
    ++s_crsf_uart_error_count;
    h723_start_uart7_receive();
}

void h723_chassis_on_fdcan1_rx(void)
{
    if (APP_H723_M2006_FDCAN_INSTANCE == 1U) {
        h723_chassis_on_fdcan_rx(&hfdcan1);
    }
}

void h723_chassis_on_fdcan2_rx(void)
{
    if (APP_H723_M2006_FDCAN_INSTANCE == 2U) {
        h723_chassis_on_fdcan_rx(&hfdcan2);
    }
}

void h723_chassis_on_fdcan3_rx(void)
{
    if (APP_H723_M2006_FDCAN_INSTANCE == 3U) {
        h723_chassis_on_fdcan_rx(&hfdcan3);
    }
}

static void h723_chassis_on_fdcan_rx(FDCAN_HandleTypeDef *fdcan)
{
    FDCAN_RxHeaderTypeDef header;
    uint8_t data[8];
    while (HAL_FDCAN_GetRxFifoFillLevel(fdcan, FDCAN_RX_FIFO0) > 0U) {
        if (HAL_FDCAN_GetRxMessage(fdcan, FDCAN_RX_FIFO0, &header, data) != HAL_OK) { break; }
        if (header.IdType == FDCAN_STANDARD_ID && header.DataLength == FDCAN_DLC_BYTES_8 && header.Identifier >= 0x201U && header.Identifier <= 0x202U &&
            app_m2006_parse_feedback(header.Identifier, data, &s_feedback[header.Identifier - 0x201U])) {
            uint32_t index = header.Identifier - 0x201U;
            s_feedback_time_ms[index] = h723_app_time_now_ms();
            s_feedback_valid[index] = true;
            ++g_h723_debug.fdcan.rx_count;
        }
    }
}

void h723_chassis_service_step(uint32_t now_ms)
{
    FDCAN_TxHeaderTypeDef header = {0};
    FDCAN_HandleTypeDef *fdcan = h723_m2006_fdcan();
    uint8_t data[8];
    int16_t output[2] = {0, 0};
    uint32_t index;
    while (s_crsf_read_index != s_crsf_write_index) {
        (void)app_crsf_parser_feed(&s_crsf_parser, s_crsf_ring[s_crsf_read_index], now_ms, &s_crsf_input);
        s_crsf_read_index = (uint16_t)((s_crsf_read_index + 1U) % H723_CRSF_RING_SIZE);
    }
    app_chassis_mix(&s_crsf_input, now_ms, &s_command);
    for (index = 0U; index < APP_CRSF_CHANNEL_COUNT; ++index) {
        g_h723_debug.crsf.channels_raw[index] = s_crsf_input.channels[index];
    }
    g_h723_debug.crsf.valid_frame_count = s_crsf_input.valid_frame_count;
    g_h723_debug.crsf.crc_error_count = s_crsf_input.crc_error_count;
    g_h723_debug.crsf.frame_error_count = s_crsf_input.frame_error_count;
    g_h723_debug.crsf.age_ms = s_crsf_input.valid ? (uint32_t)(now_ms - s_crsf_input.last_valid_ms) : UINT32_MAX;
    g_h723_debug.crsf.uart_error_count = s_crsf_uart_error_count;
    g_h723_debug.crsf.ring_overrun_count = s_crsf_ring_overrun_count;
    g_h723_debug.crsf.sb_state = h723_crsf_switch_state(s_crsf_input.channels[6]);
    g_h723_debug.crsf.sc_state = h723_crsf_switch_state(s_crsf_input.channels[7]);
    if (g_h723_debug.crsf.age_ms >= APP_H723_CRSF_TIMEOUT_MS) {
        if (!s_crsf_was_timed_out) { ++g_h723_debug.crsf.timeout_count; }
        s_crsf_was_timed_out = true;
    } else { s_crsf_was_timed_out = false; }
    g_h723_debug.chassis.mode = s_command.manual_active ? 1U : 0U;
    g_h723_debug.chassis.actuation_enabled = APP_H723_CHASSIS_ACTUATION_ENABLE;
    g_h723_debug.chassis.forward_normalized = s_command.forward_normalized;
    g_h723_debug.chassis.turn_normalized = s_command.turn_normalized;
    g_h723_debug.chassis.left_target_rpm = s_command.left_target_rpm;
    g_h723_debug.chassis.right_target_rpm = s_command.right_target_rpm;
    for (index = 0U; index < 2U; ++index) {
        float target = index == 0U ? s_command.left_target_rpm : s_command.right_target_rpm;
        bool feedback_fresh = s_feedback_valid[index] && (uint32_t)(now_ms - s_feedback_time_ms[index]) < APP_H723_M2006_FEEDBACK_TIMEOUT_MS;
        volatile h723_m2006_debug_t *debug = &g_h723_debug.m2006[index];
        debug->feedback_id = (uint16_t)(0x201U + index);
        debug->feedback_age_ms = feedback_fresh ? (uint32_t)(now_ms - s_feedback_time_ms[index]) : UINT32_MAX;
        if (s_feedback_valid[index]) { debug->encoder = s_feedback[index].encoder; debug->feedback_speed_rpm = s_feedback[index].speed_rpm; debug->feedback_current = s_feedback[index].current; debug->temperature_celsius = s_feedback[index].temperature_celsius; }
        debug->target_speed_rpm = target;
        if (!s_command.manual_active || !feedback_fresh) {
            PID_Incremental_Reset(&s_speed_pid[index]);
            output[index] = 0;
        } else {
            output[index] = h723_clamp_current(PID_Incremental_Calc(&s_speed_pid[index], target, (float)debug->feedback_speed_rpm));
        }
        debug->pid_p_out = s_speed_pid[index].p_out; debug->pid_i_out = s_speed_pid[index].i_out; debug->pid_d_out = s_speed_pid[index].d_out; debug->pid_output = s_speed_pid[index].output;
        if (APP_H723_CHASSIS_ACTUATION_ENABLE == 0U) { output[index] = 0; }
        debug->commanded_current = output[index];
    }
    app_m2006_encode_group_current(output[0], output[1], data);
    header.Identifier = 0x200U; header.IdType = FDCAN_STANDARD_ID; header.TxFrameType = FDCAN_DATA_FRAME; header.DataLength = FDCAN_DLC_BYTES_8;
    header.ErrorStateIndicator = FDCAN_ESI_ACTIVE; header.BitRateSwitch = FDCAN_BRS_OFF; header.FDFormat = FDCAN_CLASSIC_CAN; header.TxEventFifoControl = FDCAN_NO_TX_EVENTS; header.MessageMarker = 0U;
    g_h723_debug.fdcan.last_status = HAL_FDCAN_AddMessageToTxFifoQ(fdcan, &header, data);
    if (g_h723_debug.fdcan.last_status == HAL_OK) { ++g_h723_debug.fdcan.tx_count; } else { ++g_h723_debug.fdcan.tx_error_count; }
    h723_update_fdcan_diagnostics();
}
