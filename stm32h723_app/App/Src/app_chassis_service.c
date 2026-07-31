#include "app_chassis_service.h"

#include <math.h>
#include <string.h>

#include "app_balance.h"
#include "app_chassis.h"
#include "app_buttons.h"
#include "app_config.h"
#include "app_crsf.h"
#include "app_debug.h"
#include "app_line_follow.h"
#include "app_m2006.h"
#include "app_single_motor.h"
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
static app_chassis_control_state_t s_control_state;
static app_chassis_control_output_t s_control_output;
static app_m2006_feedback_t s_feedback[3];
static uint32_t s_feedback_time_ms[3];
static bool s_feedback_valid[3];
static PID_Incremental s_speed_pid[3];
static PID_Position s_position_pid[3];
static app_m2006_position_tracker_t s_position_tracker[3];
#if (APP_H723_BALANCE_ENABLE == 1U)
static app_balance_t s_balance;
#endif
#if (APP_H723_SINGLE_MOTOR_PID_DEBUG_ENABLE == 1U)
static float s_position_origin_deg[3];
#endif
static bool s_position_reference_valid[3];
static uint32_t s_crsf_uart_error_count;
static uint32_t s_crsf_ring_overrun_count;
static bool s_crsf_was_timed_out;
static uint32_t s_single_motor_last_id;
static uint32_t s_single_motor_last_enable;
static uint32_t s_single_motor_last_control_mode;
#if (APP_H723_SINGLE_MOTOR_PID_DEBUG_ENABLE == 1U)
static uint32_t s_single_motor_last_position_pid_ms;
#endif
static app_line_follow_state_t s_line_follow;
static app_line_follow_output_t s_line_follow_output;

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
    .output_limit = APP_H723_M2006_CURRENT_LIMIT_A,
    .deadband = APP_H723_M2006_PID_DEADBAND_RPM,
    .integral_output_limit = APP_H723_M2006_PID_INTEGRAL_LIMIT,
    .integral_separation_threshold = APP_H723_M2006_PID_INTEGRAL_SEPARATION_RPM,
    .derivative_filter_N = 0.0f,
    .output_delta_limit = APP_H723_M2006_PID_OUTPUT_DELTA_LIMIT,
};

static const PID_Position_Param_Config s_position_pid_params = {
    .kp = APP_H723_SINGLE_MOTOR_POSITION_PID_KP,
    .ki = APP_H723_SINGLE_MOTOR_POSITION_PID_KI,
    .kd = APP_H723_SINGLE_MOTOR_POSITION_PID_KD,
    .output_limit = APP_H723_SINGLE_MOTOR_POSITION_PID_OUTPUT_LIMIT_RPM,
    .deadband = APP_H723_SINGLE_MOTOR_POSITION_PID_DEADBAND_DEG,
};

#if (APP_H723_BALANCE_ENABLE == 1U)
static const app_balance_config_t s_balance_config = {
    .home_search_output_speed_rpm = APP_H723_BALANCE_HOME_SEARCH_OUTPUT_SPEED_RPM,
    .home_current_limit_a = APP_H723_BALANCE_HOME_CURRENT_LIMIT_A,
    .home_stall_speed_rpm = APP_H723_BALANCE_HOME_STALL_SPEED_RPM,
    .home_stall_current_ratio = APP_H723_BALANCE_HOME_STALL_CURRENT_RATIO,
    .home_confirm_ms = APP_H723_BALANCE_HOME_CONFIRM_MS,
    .home_timeout_ms = APP_H723_BALANCE_HOME_TIMEOUT_MS,
    .position_min_deg = APP_H723_BALANCE_POSITION_MIN_DEG,
    .position_active_min_deg = APP_H723_BALANCE_POSITION_ACTIVE_MIN_DEG,
    .position_max_deg = APP_H723_BALANCE_POSITION_MAX_DEG,
    .position_period_ms = APP_H723_BALANCE_POSITION_PERIOD_MS,
    .home_speed_params = {
        .kp = APP_H723_BALANCE_HOME_SPEED_PID_KP,
        .ki = APP_H723_BALANCE_HOME_SPEED_PID_KI,
        .kd = APP_H723_BALANCE_HOME_SPEED_PID_KD,
        .output_limit = APP_H723_BALANCE_HOME_CURRENT_LIMIT_A,
        .deadband = APP_H723_M2006_PID_DEADBAND_RPM,
        .derivative_filter_N = 0.0f,
        .output_delta_limit = 0.0f,
    },
    .position_speed_params = {
        .kp = APP_H723_BALANCE_POSITION_SPEED_PID_KP,
        .ki = APP_H723_BALANCE_POSITION_SPEED_PID_KI,
        .kd = APP_H723_BALANCE_POSITION_SPEED_PID_KD,
        .output_limit = APP_H723_BALANCE_POSITION_CURRENT_LIMIT_A,
        .deadband = APP_H723_M2006_PID_DEADBAND_RPM,
        .derivative_filter_N = 0.0f,
        .output_delta_limit = 0.0f,
    },
    .position_params = {
        .kp = APP_H723_BALANCE_POSITION_PID_KP,
        .ki = APP_H723_BALANCE_POSITION_PID_KI,
        .kd = APP_H723_BALANCE_POSITION_PID_KD,
        .output_limit = APP_H723_BALANCE_POSITION_MAX_OUTPUT_SPEED_RPM,
        .deadband = 0.0f,
    },
};
#endif

static const PID_Position_Param_Config s_line_follow_pid_params = {
    .kp = APP_H723_LINE_FOLLOW_PID_KP,
    .ki = APP_H723_LINE_FOLLOW_PID_KI,
    .kd = APP_H723_LINE_FOLLOW_PID_KD,
    .output_limit = APP_H723_LINE_FOLLOW_MAX_TURN_SPEED_MM_S,
    .deadband = APP_H723_LINE_FOLLOW_PID_DEADBAND,
};

static float h723_clamp_current(float value)
{
    if (value > APP_H723_M2006_CURRENT_LIMIT_A) { return APP_H723_M2006_CURRENT_LIMIT_A; }
    if (value < -APP_H723_M2006_CURRENT_LIMIT_A) { return -APP_H723_M2006_CURRENT_LIMIT_A; }
    return value;
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
    app_chassis_control_init(&s_control_state);
    for (index = 0U; index < 3U; ++index) {
        PID_Incremental_Init(&s_speed_pid[index], &s_speed_pid_params, 0.001f);
        PID_Position_Init(&s_position_pid[index], &s_position_pid_params,
                          (float)APP_H723_SINGLE_MOTOR_POSITION_PID_PERIOD_MS / 1000.0f);
        app_m2006_position_tracker_init(&s_position_tracker[index]);
    }
#if (APP_H723_BALANCE_ENABLE == 1U)
    app_balance_init(&s_balance, &s_balance_config);
#endif
    app_line_follow_init(&s_line_follow, &s_line_follow_pid_params,
                         (float)APP_GRAYSCALE_TASK_PERIOD_MS / 1000.0f);
    (void)memset(&s_line_follow_output, 0, sizeof(s_line_follow_output));
    g_h723_debug.single_motor.default_id = APP_H723_SINGLE_MOTOR_DEBUG_DEFAULT_ID;
    g_h723_debug.single_motor.selected_id = APP_H723_SINGLE_MOTOR_DEBUG_DEFAULT_ID;
    g_h723_debug.single_motor.enable = 0U;
    g_h723_debug.single_motor.control_mode = APP_SINGLE_MOTOR_CONTROL_MODE_SPEED;
    g_h723_debug.single_motor.target_output_speed_rpm = 0.0f;
    g_h723_debug.single_motor.max_target_output_speed_rpm = APP_H723_SINGLE_MOTOR_MAX_OUTPUT_RPM;
    g_h723_debug.single_motor.kp = APP_H723_M2006_PID_KP;
    g_h723_debug.single_motor.ki = APP_H723_M2006_PID_KI;
    g_h723_debug.single_motor.kd = APP_H723_M2006_PID_KD;
    g_h723_debug.single_motor.output_limit = APP_H723_M2006_CURRENT_LIMIT_A;
    g_h723_debug.single_motor.deadband = s_speed_pid_params.deadband;
    g_h723_debug.single_motor.integral_output_limit = APP_H723_M2006_PID_INTEGRAL_LIMIT;
    g_h723_debug.single_motor.integral_separation_threshold = s_speed_pid_params.integral_separation_threshold;
    g_h723_debug.single_motor.derivative_filter_N = s_speed_pid_params.derivative_filter_N;
    g_h723_debug.single_motor.output_delta_limit = APP_H723_M2006_PID_OUTPUT_DELTA_LIMIT;
    g_h723_debug.single_motor.target_position_deg = 0.0f;
    g_h723_debug.single_motor.position_kp = APP_H723_SINGLE_MOTOR_POSITION_PID_KP;
    g_h723_debug.single_motor.position_ki = APP_H723_SINGLE_MOTOR_POSITION_PID_KI;
    g_h723_debug.single_motor.position_kd = APP_H723_SINGLE_MOTOR_POSITION_PID_KD;
    g_h723_debug.single_motor.position_output_limit_rpm = APP_H723_SINGLE_MOTOR_POSITION_PID_OUTPUT_LIMIT_RPM;
    g_h723_debug.single_motor.position_deadband_deg = APP_H723_SINGLE_MOTOR_POSITION_PID_DEADBAND_DEG;
    g_h723_debug.balance.target_position_deg = 0.0f;
    g_h723_debug.balance.rehome_request = 0U;
    s_single_motor_last_id = APP_H723_SINGLE_MOTOR_DEBUG_DEFAULT_ID;
    s_single_motor_last_enable = 0U;
    s_single_motor_last_control_mode = APP_SINGLE_MOTOR_CONTROL_MODE_SPEED;
    filter.IdType = FDCAN_STANDARD_ID;
    filter.FilterIndex = 0U;
    filter.FilterType = FDCAN_FILTER_RANGE;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1 = 0x201U;
    filter.FilterID2 = 0x203U;
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
        if (header.IdType == FDCAN_STANDARD_ID && header.DataLength == FDCAN_DLC_BYTES_8 && header.Identifier >= 0x201U && header.Identifier <= 0x203U &&
            app_m2006_parse_feedback(header.Identifier, data, &s_feedback[header.Identifier - 0x201U])) {
            uint32_t index = header.Identifier - 0x201U;
            uint32_t now_ms = h723_app_time_now_ms();
            bool continuity_valid = s_feedback_valid[index] &&
                                    (uint32_t)(now_ms - s_feedback_time_ms[index]) <
                                        APP_H723_M2006_POSITION_TRACKER_MAX_GAP_MS;
            if (!continuity_valid) {
                app_m2006_position_tracker_init(&s_position_tracker[index]);
                s_position_reference_valid[index] = false;
            }
            (void)app_m2006_position_tracker_update(&s_position_tracker[index],
                                                     s_feedback[index].encoder);
            s_feedback_time_ms[index] = now_ms;
            s_feedback_valid[index] = true;
            ++g_h723_debug.fdcan.rx_count;
        }
    }
}

static bool h723_m2006_feedback_is_fresh(uint32_t index, uint32_t now_ms)
{
    return index < 3U && s_feedback_valid[index] &&
           (uint32_t)(now_ms - s_feedback_time_ms[index]) < APP_H723_M2006_FEEDBACK_TIMEOUT_MS;
}

static void h723_update_m2006_debug(uint32_t index, uint32_t now_ms, float target_rpm)
{
    volatile h723_m2006_debug_t *debug;

    if (index >= 3U) { return; }
    debug = &g_h723_debug.m2006[index];
    debug->feedback_id = (uint16_t)(0x201U + index);
    debug->feedback_age_ms = h723_m2006_feedback_is_fresh(index, now_ms) ?
                             (uint32_t)(now_ms - s_feedback_time_ms[index]) : UINT32_MAX;
    if (s_feedback_valid[index]) {
        debug->encoder = s_feedback[index].encoder;
        debug->rotor_speed_rpm = s_feedback[index].rotor_speed_rpm;
        debug->feedback_current_raw = s_feedback[index].current_raw;
        debug->feedback_output_speed_rpm = s_feedback[index].output_speed_rpm;
        debug->feedback_current_A = s_feedback[index].current_a;
        debug->temperature_celsius = s_feedback[index].temperature_celsius;
    }
    debug->target_output_speed_rpm = target_rpm;
}

#if (APP_H723_BALANCE_ENABLE == 1U)
static void h723_balance_service_step(uint32_t now_ms, float output_current_A[3])
{
    const uint32_t index = APP_H723_BALANCE_MOTOR_ID - 1U;
    const app_balance_step_input_t input = {
        .now_ms = now_ms,
        .feedback_valid = h723_m2006_feedback_is_fresh(index, now_ms),
        .feedback_position_deg = app_m2006_position_tracker_output_degrees(&s_position_tracker[index]),
        .feedback_output_speed_rpm = s_feedback[index].output_speed_rpm,
        .feedback_current_a = s_feedback[index].current_a,
        .requested_target_position_deg = g_h723_debug.balance.target_position_deg,
        .rehome_request = g_h723_debug.balance.rehome_request != 0U,
    };
    app_balance_step_output_t result;
    volatile h723_m2006_debug_t *debug = &g_h723_debug.m2006[index];

    app_balance_step(&s_balance, &input, &result);
    if (result.rehome_request_consumed) {
        g_h723_debug.balance.rehome_request = 0U;
    }
    g_h723_debug.balance.state = (uint32_t)result.state;
    g_h723_debug.balance.fault = (uint32_t)result.fault;
    g_h723_debug.balance.zero_valid = result.zero_valid ? 1U : 0U;
    g_h723_debug.balance.target_clamped = result.target_clamped ? 1U : 0U;
    g_h723_debug.balance.cycle_count++;
    g_h723_debug.balance.active_target_position_deg = result.active_target_position_deg;
    g_h723_debug.balance.zero_offset_deg = result.zero_offset_deg;
    g_h723_debug.balance.feedback_position_deg = result.feedback_position_deg;
    g_h723_debug.balance.feedback_output_speed_rpm = input.feedback_output_speed_rpm;
    g_h723_debug.balance.feedback_current_a = input.feedback_current_a;
    g_h723_debug.balance.target_output_speed_rpm = result.target_output_speed_rpm;
    g_h723_debug.balance.speed_pid_kp = s_balance.speed_pid.params.kp;
    g_h723_debug.balance.speed_pid_ki = s_balance.speed_pid.params.ki;
    g_h723_debug.balance.speed_pid_kd = s_balance.speed_pid.params.kd;
    g_h723_debug.balance.speed_pid_dt_s = s_balance.speed_pid.dt_s;
    g_h723_debug.balance.speed_pid_error_rpm = s_balance.speed_pid.error;
    g_h723_debug.balance.speed_pid_integral_output_a = s_balance.speed_pid.integral_output;
    g_h723_debug.balance.speed_pid_raw_output_a = s_balance.speed_pid.raw_output;
    g_h723_debug.balance.speed_pid_p_out_a = s_balance.speed_pid.p_out;
    g_h723_debug.balance.speed_pid_i_out_a = s_balance.speed_pid.i_out;
    g_h723_debug.balance.speed_pid_d_out_a = s_balance.speed_pid.d_out;
    g_h723_debug.balance.speed_pid_output_a = s_balance.speed_pid.output;
    h723_update_m2006_debug(index, now_ms, result.target_output_speed_rpm);
    output_current_A[index] = APP_H723_CHASSIS_ACTUATION_ENABLE == 1U ?
                              result.commanded_current_a : 0.0f;
    debug->pid_raw_output_A = s_balance.speed_pid.raw_output;
    debug->pid_p_out_A = s_balance.speed_pid.p_out;
    debug->pid_i_out_A = s_balance.speed_pid.i_out;
    debug->pid_d_out_A = s_balance.speed_pid.d_out;
    debug->pid_output_A = s_balance.speed_pid.output;
    debug->commanded_current_raw = app_m2006_current_a_to_raw(output_current_A[index]);
    debug->commanded_current_A = app_m2006_raw_current_to_a(debug->commanded_current_raw);
    output_current_A[index] = debug->commanded_current_A;
    g_h723_debug.balance.commanded_current_raw = debug->commanded_current_raw;
    g_h723_debug.balance.commanded_current_a = debug->commanded_current_A;
}
#else
static void h723_balance_service_step(uint32_t now_ms, float output_current_A[3])
{
    (void)now_ms;
    (void)output_current_A;
}
#endif

#if (APP_H723_SINGLE_MOTOR_PID_DEBUG_ENABLE == 1U)
static void h723_single_motor_service_step(uint32_t now_ms, float output_current_A[3])
{
    app_single_motor_step_input_t input = {0};
    app_single_motor_step_output_t result = {0};
    PID_Incremental_Param_Config params = s_speed_pid_params;
    PID_Position_Param_Config position_params = s_position_pid_params;
    uint32_t selected_id;
    uint32_t selected_index;
    uint32_t index;
    bool position_reference_just_set = false;
    float max_target_output_speed_rpm;

    selected_id = app_single_motor_sanitize_id(g_h723_debug.single_motor.selected_id,
                                                APP_H723_SINGLE_MOTOR_DEBUG_DEFAULT_ID);
    if (selected_id == APP_H723_BALANCE_MOTOR_ID) {
        selected_id = APP_H723_SINGLE_MOTOR_DEBUG_DEFAULT_ID;
    }
    selected_index = selected_id - 1U;
    g_h723_debug.single_motor.selected_id = selected_id;
    max_target_output_speed_rpm = app_single_motor_sanitize_max_output_speed_rpm(
        g_h723_debug.single_motor.max_target_output_speed_rpm);
    g_h723_debug.single_motor.max_target_output_speed_rpm = max_target_output_speed_rpm;
    input.enable = g_h723_debug.single_motor.enable;
    input.control_mode = g_h723_debug.single_motor.control_mode;
    input.selected_id = selected_id;
    input.default_id = APP_H723_SINGLE_MOTOR_DEBUG_DEFAULT_ID;
    input.target_output_speed_rpm = g_h723_debug.single_motor.target_output_speed_rpm;
    params.kp = g_h723_debug.single_motor.kp;
    params.ki = g_h723_debug.single_motor.ki;
    params.kd = g_h723_debug.single_motor.kd;
    params.output_limit = g_h723_debug.single_motor.output_limit;
    params.deadband = g_h723_debug.single_motor.deadband;
    params.integral_output_limit = g_h723_debug.single_motor.integral_output_limit;
    params.integral_separation_threshold = g_h723_debug.single_motor.integral_separation_threshold;
    params.derivative_filter_N = g_h723_debug.single_motor.derivative_filter_N;
    params.output_delta_limit = g_h723_debug.single_motor.output_delta_limit;
    input.params = params;
    input.target_position_deg = g_h723_debug.single_motor.target_position_deg;
    position_params.kp = g_h723_debug.single_motor.position_kp;
    position_params.ki = g_h723_debug.single_motor.position_ki;
    position_params.kd = g_h723_debug.single_motor.position_kd;
    position_params.output_limit = g_h723_debug.single_motor.position_output_limit_rpm;
    position_params.deadband = g_h723_debug.single_motor.position_deadband_deg;
    input.position_params = position_params;
    input.configuration_changed = selected_id != s_single_motor_last_id ||
                                  (input.enable == 1U) != (s_single_motor_last_enable == 1U) ||
                                  input.control_mode != s_single_motor_last_control_mode;
    input.feedback_valid = h723_m2006_feedback_is_fresh(selected_index, now_ms);
    input.feedback_age_ms = h723_m2006_feedback_is_fresh(selected_index, now_ms) ?
                            (uint32_t)(now_ms - s_feedback_time_ms[selected_index]) : UINT32_MAX;
    input.feedback_output_speed_rpm = s_feedback[selected_index].output_speed_rpm;
    input.feedback_position_deg = app_m2006_position_tracker_output_degrees(
        &s_position_tracker[selected_index]) - s_position_origin_deg[selected_index];
    input.position_reference_valid = s_position_reference_valid[selected_index];
    input.position_update_due = (uint32_t)(now_ms - s_single_motor_last_position_pid_ms) >=
                                APP_H723_SINGLE_MOTOR_POSITION_PID_PERIOD_MS;
    if (input.configuration_changed) {
        s_position_reference_valid[selected_index] = false;
        input.position_reference_valid = false;
        s_single_motor_last_position_pid_ms = now_ms;
    } else if (input.control_mode == APP_SINGLE_MOTOR_CONTROL_MODE_POSITION &&
               !input.position_reference_valid && input.feedback_valid &&
               s_position_tracker[selected_index].initialized) {
        s_position_origin_deg[selected_index] = app_m2006_position_tracker_output_degrees(
            &s_position_tracker[selected_index]);
        s_position_reference_valid[selected_index] = true;
        input.position_reference_valid = true;
        input.feedback_position_deg = 0.0f;
        position_reference_just_set = true;
        s_single_motor_last_position_pid_ms = now_ms;
    }
    s_single_motor_last_id = selected_id;
    s_single_motor_last_enable = input.enable;
    s_single_motor_last_control_mode = input.control_mode;

    for (index = 0U; index < 3U; ++index) {
        h723_update_m2006_debug(index, now_ms, 0.0f);
        if (index != selected_index) {
            PID_Incremental_Reset(&s_speed_pid[index]);
            PID_Position_Reset(&s_position_pid[index]);
            g_h723_debug.m2006[index].pid_raw_output_A = 0.0f;
            g_h723_debug.m2006[index].pid_p_out_A = 0.0f;
            g_h723_debug.m2006[index].pid_i_out_A = 0.0f;
            g_h723_debug.m2006[index].pid_d_out_A = 0.0f;
            g_h723_debug.m2006[index].pid_output_A = 0.0f;
            g_h723_debug.m2006[index].commanded_current_raw = 0;
            g_h723_debug.m2006[index].commanded_current_A = 0.0f;
        }
        output_current_A[index] = 0.0f;
    }
    if (!input.configuration_changed && !position_reference_just_set) {
        (void)app_single_motor_step(&s_speed_pid[selected_index], &s_position_pid[selected_index], &input,
                                    APP_H723_M2006_FEEDBACK_TIMEOUT_MS,
                                    max_target_output_speed_rpm,
                                    APP_H723_M2006_CURRENT_LIMIT_A, &result);
        if (input.control_mode == APP_SINGLE_MOTOR_CONTROL_MODE_POSITION && input.position_update_due) {
            s_single_motor_last_position_pid_ms = now_ms;
        }
    } else {
        PID_Incremental_Reset(&s_speed_pid[selected_index]);
        PID_Position_Reset(&s_position_pid[selected_index]);
        result.selected_id = selected_id;
        result.reset_pid = true;
        result.safety_reason = APP_SINGLE_MOTOR_SAFETY_CONFIGURATION_CHANGED;
    }
    if (result.active) {
        output_current_A[selected_index] = result.commanded_current_A;
    }

    g_h723_debug.single_motor.active = result.active ? 1U : 0U;
    g_h723_debug.single_motor.reset_pid = result.reset_pid ? 1U : 0U;
    g_h723_debug.single_motor.safety_reason = result.safety_reason;
    g_h723_debug.single_motor.cycle_count++;
    g_h723_debug.single_motor.feedback_encoder = g_h723_debug.m2006[selected_index].encoder;
    g_h723_debug.single_motor.rotor_speed_rpm = g_h723_debug.m2006[selected_index].rotor_speed_rpm;
    g_h723_debug.single_motor.feedback_output_speed_rpm = g_h723_debug.m2006[selected_index].feedback_output_speed_rpm;
    g_h723_debug.single_motor.feedback_current_raw = g_h723_debug.m2006[selected_index].feedback_current_raw;
    g_h723_debug.single_motor.feedback_current_A = g_h723_debug.m2006[selected_index].feedback_current_A;
    g_h723_debug.single_motor.feedback_temperature_celsius = g_h723_debug.m2006[selected_index].temperature_celsius;
    g_h723_debug.single_motor.feedback_age_ms = g_h723_debug.m2006[selected_index].feedback_age_ms;
    g_h723_debug.single_motor.feedback_position_deg = input.feedback_position_deg;
    g_h723_debug.single_motor.position_reference_valid = input.position_reference_valid ? 1U : 0U;
    if (input.control_mode == APP_SINGLE_MOTOR_CONTROL_MODE_POSITION && input.position_update_due &&
        result.active) {
        g_h723_debug.single_motor.position_cycle_count++;
    }
    g_h723_debug.single_motor.position_target_output_speed_rpm = result.position_output_rpm;
    g_h723_debug.single_motor.position_p_out_rpm = result.position_p_out_rpm;
    g_h723_debug.single_motor.position_i_out_rpm = result.position_i_out_rpm;
    g_h723_debug.single_motor.position_d_out_rpm = result.position_d_out_rpm;
    g_h723_debug.single_motor.position_output_rpm = result.position_output_rpm;
    g_h723_debug.single_motor.target_current_A = output_current_A[selected_index];
    g_h723_debug.single_motor.target_current_raw = app_m2006_current_a_to_raw(output_current_A[selected_index]);
    g_h723_debug.single_motor.target_current_A = app_m2006_raw_current_to_a(g_h723_debug.single_motor.target_current_raw);
    g_h723_debug.single_motor.pid_raw_output_A = result.raw_output_A;
    g_h723_debug.single_motor.pid_p_out_A = result.pid_p_out_A;
    g_h723_debug.single_motor.pid_i_out_A = result.pid_i_out_A;
    g_h723_debug.single_motor.pid_d_out_A = result.pid_d_out_A;
    g_h723_debug.single_motor.pid_output_A = result.pid_output_A;
    g_h723_debug.m2006[selected_index].pid_p_out_A = result.pid_p_out_A;
    g_h723_debug.m2006[selected_index].pid_i_out_A = result.pid_i_out_A;
    g_h723_debug.m2006[selected_index].pid_d_out_A = result.pid_d_out_A;
    g_h723_debug.m2006[selected_index].pid_output_A = result.pid_output_A;
    g_h723_debug.m2006[selected_index].target_output_speed_rpm = result.target_output_speed_rpm;
    g_h723_debug.m2006[selected_index].commanded_current_raw = g_h723_debug.single_motor.target_current_raw;
    g_h723_debug.m2006[selected_index].commanded_current_A = g_h723_debug.single_motor.target_current_A;
}
#endif

void h723_chassis_service_step(uint32_t now_ms)
{
    FDCAN_TxHeaderTypeDef header = {0};
    FDCAN_HandleTypeDef *fdcan = h723_m2006_fdcan();
    uint8_t data[8];
    float output_current_A[3] = {0.0f, 0.0f, 0.0f};
    int16_t output_raw[3] = {0, 0, 0};
    uint32_t index;
    h723_app_buttons_snapshot_t button_snapshot;
    while (s_crsf_read_index != s_crsf_write_index) {
        (void)app_crsf_parser_feed(&s_crsf_parser, s_crsf_ring[s_crsf_read_index], now_ms, &s_crsf_input);
        s_crsf_read_index = (uint16_t)((s_crsf_read_index + 1U) % H723_CRSF_RING_SIZE);
    }
    h723_app_buttons_snapshot_copy(&button_snapshot);
    app_chassis_control_step(&s_control_state, &s_crsf_input,
                             button_snapshot.stable_high_mask, now_ms,
                             &s_control_output);
    s_command = s_control_output.chassis;
    for (index = 0U; index < APP_CRSF_CHANNEL_COUNT; ++index) {
        g_h723_debug.crsf.channels_raw[index] = s_crsf_input.channels[index];
    }
    g_h723_debug.crsf.valid_frame_count = s_crsf_input.valid_frame_count;
    g_h723_debug.crsf.crc_error_count = s_crsf_input.crc_error_count;
    g_h723_debug.crsf.frame_error_count = s_crsf_input.frame_error_count;
    g_h723_debug.crsf.age_ms = s_crsf_input.valid ? (uint32_t)(now_ms - s_crsf_input.last_valid_ms) : UINT32_MAX;
    g_h723_debug.crsf.uart_error_count = s_crsf_uart_error_count;
    g_h723_debug.crsf.ring_overrun_count = s_crsf_ring_overrun_count;
    g_h723_debug.crsf.sb_state = h723_crsf_switch_state(
        s_crsf_input.channels[APP_H723_CRSF_SB_CHANNEL_INDEX]);
    g_h723_debug.crsf.sc_state = h723_crsf_switch_state(
        s_crsf_input.channels[APP_H723_CRSF_SC_CHANNEL_INDEX]);
    if (g_h723_debug.crsf.age_ms >= APP_H723_CRSF_TIMEOUT_MS) {
        if (!s_crsf_was_timed_out) { ++g_h723_debug.crsf.timeout_count; }
        s_crsf_was_timed_out = true;
    } else { s_crsf_was_timed_out = false; }
    g_h723_debug.chassis.mode = (uint32_t)s_command.mode;
    g_h723_debug.chassis.actuation_enabled = APP_H723_CHASSIS_ACTUATION_ENABLE;
    g_h723_debug.chassis.forward_normalized = s_command.forward_normalized;
    g_h723_debug.chassis.turn_normalized = s_command.turn_normalized;
    g_h723_debug.chassis.base_speed_mm_s = s_command.base_speed_mm_s;
    g_h723_debug.chassis.line_position = g_h723_debug.grayscale.line_position;
    g_h723_debug.chassis.line_strength = g_h723_debug.grayscale.line_strength;
    g_h723_debug.chassis.line_valid = 0U;
    g_h723_debug.chassis.line_turn_correction_mm_s = 0.0f;
    g_h723_debug.chassis.left_target_output_speed_rpm = s_command.left_target_rpm;
    g_h723_debug.chassis.right_target_output_speed_rpm = s_command.right_target_rpm;
    g_h723_debug.control.mode = (uint32_t)s_control_output.mode;
    g_h723_debug.control.remote_takeover = s_control_output.remote_takeover ? 1U : 0U;
    g_h723_debug.control.buttons_enabled = s_control_output.buttons_enabled ? 1U : 0U;
    g_h723_debug.control.se_pressed = s_control_output.se_pressed ? 1U : 0U;
    g_h723_debug.control.sb_state = s_control_output.sb_state;
    g_h723_debug.control.sc_state = s_control_output.sc_state;
    g_h723_debug.control.button_stable_high_mask = button_snapshot.stable_high_mask;
    g_h723_debug.control.selected_task = s_control_output.selected_task;
    g_h723_debug.control.task_request_available =
        s_control_output.task_request_available ? 1U : 0U;
#if (APP_H723_SINGLE_MOTOR_PID_DEBUG_ENABLE == 1U)
    g_h723_debug.chassis.mode = 2U;
    g_h723_debug.chassis.actuation_enabled = g_h723_debug.single_motor.enable;
    g_h723_debug.chassis.forward_normalized = 0.0f;
    g_h723_debug.chassis.turn_normalized = 0.0f;
    g_h723_debug.chassis.left_target_output_speed_rpm = 0.0f;
    g_h723_debug.chassis.right_target_output_speed_rpm = 0.0f;
    h723_single_motor_service_step(now_ms, output_current_A);
#else
    if (s_command.mode == APP_CHASSIS_MODE_LINE_FOLLOW && s_command.manual_active) {
        const app_line_follow_input_t line_input = {
            .line_position = g_h723_debug.grayscale.line_position,
            .line_strength = g_h723_debug.grayscale.line_strength,
            .adc_timeout_mask = g_h723_debug.grayscale.adc_timeout_mask,
            .sequence = g_h723_debug.grayscale.sequence,
            .base_speed_mm_s = s_command.base_speed_mm_s,
        };

        app_line_follow_step(&s_line_follow, &line_input, &s_line_follow_output);
        if (s_line_follow_output.active) {
            s_command.left_target_rpm = s_line_follow_output.left_target_rpm;
            s_command.right_target_rpm = s_line_follow_output.right_target_rpm;
            g_h723_debug.chassis.line_valid = 1U;
            g_h723_debug.chassis.line_turn_correction_mm_s =
                s_line_follow_output.turn_correction_mm_s;
        } else {
            s_command.left_target_rpm = 0.0f;
            s_command.right_target_rpm = 0.0f;
        }
    } else {
        app_line_follow_reset(&s_line_follow);
        (void)memset(&s_line_follow_output, 0, sizeof(s_line_follow_output));
    }

    for (index = 0U; index < 3U; ++index) {
        float target = index == 0U ? s_command.left_target_rpm : s_command.right_target_rpm;
        bool feedback_fresh = h723_m2006_feedback_is_fresh(index, now_ms);
        volatile h723_m2006_debug_t *debug = &g_h723_debug.m2006[index];
        if (index == 2U) { target = 0.0f; }
        h723_update_m2006_debug(index, now_ms, target);
        debug->target_output_speed_rpm = target;
        if (index == 2U || !s_command.manual_active ||
            (s_command.mode == APP_CHASSIS_MODE_LINE_FOLLOW &&
             !s_line_follow_output.active) || !feedback_fresh) {
            PID_Incremental_Reset(&s_speed_pid[index]);
            output_current_A[index] = 0.0f;
        } else {
            output_current_A[index] = h723_clamp_current(PID_Incremental_Calc(&s_speed_pid[index], target, debug->feedback_output_speed_rpm));
        }
        debug->pid_raw_output_A = s_speed_pid[index].raw_output; debug->pid_p_out_A = s_speed_pid[index].p_out; debug->pid_i_out_A = s_speed_pid[index].i_out; debug->pid_d_out_A = s_speed_pid[index].d_out; debug->pid_output_A = s_speed_pid[index].output;
        if (APP_H723_CHASSIS_ACTUATION_ENABLE == 0U) { output_current_A[index] = 0.0f; }
        debug->commanded_current_raw = app_m2006_current_a_to_raw(output_current_A[index]);
        debug->commanded_current_A = app_m2006_raw_current_to_a(debug->commanded_current_raw);
        output_current_A[index] = debug->commanded_current_A;
    }
#endif
    h723_balance_service_step(now_ms, output_current_A);
    g_h723_debug.chassis.left_target_output_speed_rpm = s_command.left_target_rpm;
    g_h723_debug.chassis.right_target_output_speed_rpm = s_command.right_target_rpm;
    g_h723_debug.chassis.left_target_speed_mm_s =
        s_command.left_target_rpm * APP_H723_OUTPUT_RPM_TO_MM_S;
    g_h723_debug.chassis.right_target_speed_mm_s =
        s_command.right_target_rpm * APP_H723_OUTPUT_RPM_TO_MM_S;
    for (index = 0U; index < 3U; ++index) {
        output_raw[index] = app_m2006_current_a_to_raw(output_current_A[index]);
    }
    app_m2006_encode_group_current_slots(output_raw, data);
    header.Identifier = 0x200U; header.IdType = FDCAN_STANDARD_ID; header.TxFrameType = FDCAN_DATA_FRAME; header.DataLength = FDCAN_DLC_BYTES_8;
    header.ErrorStateIndicator = FDCAN_ESI_ACTIVE; header.BitRateSwitch = FDCAN_BRS_OFF; header.FDFormat = FDCAN_CLASSIC_CAN; header.TxEventFifoControl = FDCAN_NO_TX_EVENTS; header.MessageMarker = 0U;
    g_h723_debug.fdcan.last_status = HAL_FDCAN_AddMessageToTxFifoQ(fdcan, &header, data);
    if (g_h723_debug.fdcan.last_status == HAL_OK) { ++g_h723_debug.fdcan.tx_count; } else { ++g_h723_debug.fdcan.tx_error_count; }
    h723_update_fdcan_diagnostics();
}
