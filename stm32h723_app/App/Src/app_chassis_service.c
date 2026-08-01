#include "app_chassis_service.h"

#include <limits.h>
#include <math.h>
#include <string.h>

#include "app_balance.h"
#include "app_ball_position_control.h"
#include "app_chassis.h"
#include "app_buttons.h"
#include "app_config.h"
#include "app_crsf.h"
#include "app_debug.h"
#include "app_line_follow.h"
#include "app_jy901s_service.h"
#include "app_m2006.h"
#include "app_pipe_startup.h"
#include "app_single_motor.h"
#include "app_speed_profile.h"
#include "app_task2.h"
#include "app_task4.h"
#include "app_task56.h"
#include "app_task_menu.h"
#include "app_time.h"
#include "app_tilt_control.h"
#include "app_k230_service.h"
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
#if (APP_H723_TILT_CONTROL_ENABLE == 1U)
static app_tilt_control_t s_tilt_control;
#endif
static app_ball_position_control_t s_ball_position_control;
static app_ball_position_control_config_t s_ball_position_config;
static app_ball_position_control_output_t s_ball_position_output;
static app_ball_position_control_t s_ball_position_dynamic_control;
static app_ball_position_control_config_t s_ball_position_dynamic_config;
static bool s_ball_position_dynamic_active;
static app_pipe_startup_t s_pipe_startup;
static app_pipe_startup_snapshot_t s_pipe_startup_snapshot;
static bool s_pipe_button_release_required;
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
static uint32_t s_line_follow_debug_last_sequence;
static uint32_t s_line_follow_active_group = APP_H723_LINE_FOLLOW_GROUP_COMMON;
static app_speed_profile_t s_remote_ball_speed_profile;
static app_speed_profile_output_t s_remote_ball_speed_profile_output;
static app_task2_state_t s_task2;
static app_task2_output_t s_task2_output;
static app_task4_state_t s_task4;
static app_task4_output_t s_task4_output;
static app_task56_state_t s_task56;
static app_task56_output_t s_task56_output;
static uint32_t s_task56_task_id;

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
    .position_debug_active_min_deg = APP_H723_BALANCE_POSITION_DEBUG_ACTIVE_MIN_DEG,
    .position_debug_max_deg = APP_H723_BALANCE_POSITION_DEBUG_MAX_DEG,
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
#if (APP_H723_TILT_CONTROL_ENABLE == 1U)
static const app_tilt_control_config_t s_tilt_control_config = {
    .position_min_deg = APP_H723_BALANCE_POSITION_ACTIVE_MIN_DEG,
    .position_max_deg = APP_H723_BALANCE_POSITION_MAX_DEG,
    .sample_period_ms = APP_H723_TILT_CONTROL_PERIOD_MS,
    .sample_max_age_ms = APP_H723_TILT_CONTROL_SAMPLE_MAX_AGE_MS,
    .pitch_to_tilt_sign = APP_H723_TILT_CONTROL_PITCH_TO_TILT_SIGN,
    .derivative_filter_N = APP_H723_TILT_CONTROL_DERIVATIVE_FILTER_N,
    .pid_params = {
        .kp = APP_H723_TILT_CONTROL_PID_KP,
        .ki = APP_H723_TILT_CONTROL_PID_KI,
        .kd = APP_H723_TILT_CONTROL_PID_KD,
        .output_limit = APP_H723_TILT_CONTROL_MAX_POSITION_RATE_DEG_S,
        .deadband = APP_H723_TILT_CONTROL_PID_DEADBAND_DEG,
    },
};
#endif
#endif

static const PID_Position_Param_Config s_line_follow_common_pid_params = {
    .kp = APP_H723_LINE_FOLLOW_PID_KP,
    .ki = APP_H723_LINE_FOLLOW_PID_KI,
    .kd = APP_H723_LINE_FOLLOW_PID_KD,
    .output_limit = APP_H723_LINE_FOLLOW_MAX_TURN_SPEED_MM_S,
    .deadband = APP_H723_LINE_FOLLOW_PID_DEADBAND,
};

static const PID_Position_Param_Config s_line_follow_task2_pid_defaults = {
    .kp = APP_H723_TASK2_LINE_FOLLOW_PID_KP,
    .ki = APP_H723_TASK2_LINE_FOLLOW_PID_KI,
    .kd = APP_H723_TASK2_LINE_FOLLOW_PID_KD,
    .output_limit = APP_H723_TASK2_LINE_FOLLOW_PID_OUTPUT_LIMIT_MM_S,
    .deadband = APP_H723_TASK2_LINE_FOLLOW_PID_DEADBAND,
};

static const PID_Position_Param_Config s_line_follow_task456_pid_defaults = {
    .kp = APP_H723_TASK456_LINE_FOLLOW_PID_KP,
    .ki = APP_H723_TASK456_LINE_FOLLOW_PID_KI,
    .kd = APP_H723_TASK456_LINE_FOLLOW_PID_KD,
    .output_limit = APP_H723_TASK456_LINE_FOLLOW_PID_OUTPUT_LIMIT_MM_S,
    .deadband = APP_H723_TASK456_LINE_FOLLOW_PID_DEADBAND,
};

static bool h723_line_follow_pid_params_are_valid(float kp, float ki, float kd,
                                                  float output_limit, float deadband)
{
    return isfinite(kp) && isfinite(ki) && isfinite(kd) && isfinite(output_limit) &&
           isfinite(deadband) && kp >= 0.0f && ki >= 0.0f && kd >= 0.0f &&
           output_limit > 0.0f && deadband >= 0.0f;
}

static bool h723_line_follow_validate_task_params(
    volatile h723_debug_line_follow_params_t *debug)
{
    const bool valid = h723_line_follow_pid_params_are_valid(
        debug->pid_kp, debug->pid_ki, debug->pid_kd,
        debug->pid_output_limit_mm_s, debug->pid_deadband);

    if (valid) {
        debug->params_valid = 1U;
    } else {
        debug->params_valid = 0U;
        debug->params_rejected_count++;
    }
    return valid;
}

static bool h723_line_follow_validate_common_params(
    volatile h723_debug_line_follow_t *debug)
{
    const bool valid = h723_line_follow_pid_params_are_valid(
        debug->pid_kp, debug->pid_ki, debug->pid_kd,
        debug->pid_output_limit_mm_s, debug->pid_deadband);

    if (valid) {
        debug->params_valid = 1U;
    } else {
        debug->params_valid = 0U;
        debug->params_rejected_count++;
    }
    return valid;
}

static void h723_line_follow_apply_pid_params(
    const PID_Position_Param_Config *defaults,
    float kp, float ki, float kd, float output_limit, float deadband)
{
    if (defaults == NULL) {
        return;
    }
    s_line_follow.pid.params = *defaults;
    s_line_follow.pid.params.kp = kp;
    s_line_follow.pid.params.ki = ki;
    s_line_follow.pid.params.kd = kd;
    s_line_follow.pid.params.output_limit = output_limit;
    s_line_follow.pid.params.deadband = deadband;
}

static void h723_line_follow_apply_debug_params(uint32_t active_group)
{
    volatile h723_debug_line_follow_t *common_debug = &g_h723_debug.line_follow;
    volatile h723_debug_line_follow_params_t *task2_debug =
        &g_h723_debug.task2_line_follow;
    volatile h723_debug_line_follow_params_t *task456_debug =
        &g_h723_debug.task456_line_follow;
    const bool common_valid = h723_line_follow_validate_common_params(common_debug);
    const bool task2_valid = h723_line_follow_validate_task_params(task2_debug);
    const bool task456_valid = h723_line_follow_validate_task_params(task456_debug);

    s_line_follow_active_group = active_group;
    if (active_group == APP_H723_LINE_FOLLOW_GROUP_TASK2) {
        if (task2_valid) {
            h723_line_follow_apply_pid_params(
                &s_line_follow_task2_pid_defaults,
                task2_debug->pid_kp, task2_debug->pid_ki, task2_debug->pid_kd,
                task2_debug->pid_output_limit_mm_s, task2_debug->pid_deadband);
        } else {
            s_line_follow.pid.params = s_line_follow_task2_pid_defaults;
        }
        if (task2_debug->reset_pid_request != 0U) {
            app_line_follow_reset(&s_line_follow);
            s_line_follow_debug_last_sequence = 0U;
            task2_debug->reset_pid_request = 0U;
        }
    } else if (active_group == APP_H723_LINE_FOLLOW_GROUP_TASK456) {
        if (task456_valid) {
            h723_line_follow_apply_pid_params(
                &s_line_follow_task456_pid_defaults,
                task456_debug->pid_kp, task456_debug->pid_ki,
                task456_debug->pid_kd, task456_debug->pid_output_limit_mm_s,
                task456_debug->pid_deadband);
        } else {
            s_line_follow.pid.params = s_line_follow_task456_pid_defaults;
        }
        if (task456_debug->reset_pid_request != 0U) {
            app_line_follow_reset(&s_line_follow);
            s_line_follow_debug_last_sequence = 0U;
            task456_debug->reset_pid_request = 0U;
        }
    } else {
        s_line_follow_active_group = APP_H723_LINE_FOLLOW_GROUP_COMMON;
        if (common_valid) {
            h723_line_follow_apply_pid_params(
                &s_line_follow_common_pid_params,
                common_debug->pid_kp, common_debug->pid_ki, common_debug->pid_kd,
                common_debug->pid_output_limit_mm_s, common_debug->pid_deadband);
        } else {
            s_line_follow.pid.params = s_line_follow_common_pid_params;
        }
        if (common_debug->reset_pid_request != 0U) {
            app_line_follow_reset(&s_line_follow);
            s_line_follow_debug_last_sequence = 0U;
            common_debug->reset_pid_request = 0U;
        }
    }
}

static void h723_line_follow_publish_debug(void)
{
    volatile h723_debug_line_follow_t *debug = &g_h723_debug.line_follow;
    const PID_Position *pid = &s_line_follow.pid;
    const bool active = s_line_follow_output.active;

    debug->active = active ? 1U : 0U;
    debug->line_valid = s_line_follow_output.line_valid ? 1U : 0U;
    debug->adc_timeout_mask = g_h723_debug.grayscale.adc_timeout_mask;
    debug->line_strength = g_h723_debug.grayscale.line_strength;
    debug->sequence = g_h723_debug.grayscale.sequence;
    debug->line_position = g_h723_debug.grayscale.line_position;
    debug->base_speed_mm_s = s_command.base_speed_mm_s;
    debug->turn_correction_mm_s = s_line_follow_output.turn_correction_mm_s;
    debug->left_target_speed_mm_s =
        s_command.left_target_rpm * APP_H723_OUTPUT_RPM_TO_MM_S;
    debug->right_target_speed_mm_s =
        s_command.right_target_rpm * APP_H723_OUTPUT_RPM_TO_MM_S;
    debug->pid_dt_s = pid->dt_s;
    debug->target_position = 0.0f;
    debug->error = pid->last_error;
    debug->integral = pid->integral;
    debug->p_out = pid->p_out;
    debug->i_out = pid->i_out;
    debug->d_out = pid->d_out;
    debug->raw_output = pid->p_out + pid->i_out + pid->d_out;
    debug->pid_output = pid->output;
    debug->active_group = s_line_follow_active_group;

    if (!active) {
        s_line_follow_debug_last_sequence = 0U;
    } else if (s_line_follow_output.sequence != s_line_follow_debug_last_sequence) {
        s_line_follow_debug_last_sequence = s_line_follow_output.sequence;
        debug->pid_update_count++;
    }
}

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
#if (APP_H723_TILT_CONTROL_ENABLE == 1U)
    app_tilt_control_init(&s_tilt_control, &s_tilt_control_config);
#endif
#endif
    app_ball_position_control_config_default(&s_ball_position_config);
    s_ball_position_config.period_ms = APP_H723_BALL_POSITION_PERIOD_MS;
    s_ball_position_config.max_age_ms = APP_H723_BALL_POSITION_SAMPLE_MAX_AGE_MS;
    app_ball_position_control_init(&s_ball_position_control, &s_ball_position_config);
    app_ball_position_control_config_default(&s_ball_position_dynamic_config);
    s_ball_position_dynamic_config.period_ms = APP_H723_BALL_POSITION_PERIOD_MS;
    s_ball_position_dynamic_config.max_age_ms = APP_H723_BALL_POSITION_SAMPLE_MAX_AGE_MS;
    app_ball_position_control_init(&s_ball_position_dynamic_control,
                                   &s_ball_position_dynamic_config);
    s_ball_position_dynamic_active = false;
    app_pipe_startup_init(&s_pipe_startup);
    (void)memset(&s_pipe_startup_snapshot, 0, sizeof(s_pipe_startup_snapshot));
    s_pipe_startup_snapshot.state = APP_PIPE_STARTUP_STATE_WAIT_HOME;
    s_pipe_startup_snapshot.other_motors_allowed = true;
    s_pipe_button_release_required = false;
    app_line_follow_init(&s_line_follow, &s_line_follow_common_pid_params,
                         (float)APP_GRAYSCALE_TASK_PERIOD_MS / 1000.0f);
    app_task2_init(&s_task2);
    (void)memset(&s_task2_output, 0, sizeof(s_task2_output));
    app_task4_init(&s_task4);
    (void)memset(&s_task4_output, 0, sizeof(s_task4_output));
    app_task56_init(&s_task56);
    (void)memset(&s_task56_output, 0, sizeof(s_task56_output));
    (void)memset(&s_line_follow_output, 0, sizeof(s_line_follow_output));
    s_line_follow_debug_last_sequence = 0U;
    s_line_follow_active_group = APP_H723_LINE_FOLLOW_GROUP_COMMON;
    app_speed_profile_init(&s_remote_ball_speed_profile,
                           &(const app_speed_profile_config_t){
                               .max_speed_mm_s =
                                   APP_H723_REMOTE_BALL_SPEED_PROFILE_MAX_SPEED_MM_S,
                               .max_accel_mm_s2 =
                                   APP_H723_REMOTE_BALL_SPEED_PROFILE_MAX_ACCEL_MM_S2,
                               .max_jerk_mm_s3 =
                                   APP_H723_REMOTE_BALL_SPEED_PROFILE_MAX_JERK_MM_S3,
                           });
    (void)memset(&s_remote_ball_speed_profile_output, 0,
                 sizeof(s_remote_ball_speed_profile_output));
    g_h723_debug.task2_line_follow.pid_kp = APP_H723_TASK2_LINE_FOLLOW_PID_KP;
    g_h723_debug.task2_line_follow.pid_ki = APP_H723_TASK2_LINE_FOLLOW_PID_KI;
    g_h723_debug.task2_line_follow.pid_kd = APP_H723_TASK2_LINE_FOLLOW_PID_KD;
    g_h723_debug.task2_line_follow.pid_output_limit_mm_s =
        APP_H723_TASK2_LINE_FOLLOW_PID_OUTPUT_LIMIT_MM_S;
    g_h723_debug.task2_line_follow.pid_deadband =
        APP_H723_TASK2_LINE_FOLLOW_PID_DEADBAND;
    g_h723_debug.task2_line_follow.reset_pid_request = 0U;
    g_h723_debug.task2_line_follow.params_valid = 1U;
    g_h723_debug.task2_line_follow.params_rejected_count = 0U;
    g_h723_debug.task456_line_follow.pid_kp = APP_H723_TASK456_LINE_FOLLOW_PID_KP;
    g_h723_debug.task456_line_follow.pid_ki = APP_H723_TASK456_LINE_FOLLOW_PID_KI;
    g_h723_debug.task456_line_follow.pid_kd = APP_H723_TASK456_LINE_FOLLOW_PID_KD;
    g_h723_debug.task456_line_follow.pid_output_limit_mm_s =
        APP_H723_TASK456_LINE_FOLLOW_PID_OUTPUT_LIMIT_MM_S;
    g_h723_debug.task456_line_follow.pid_deadband =
        APP_H723_TASK456_LINE_FOLLOW_PID_DEADBAND;
    g_h723_debug.task456_line_follow.reset_pid_request = 0U;
    g_h723_debug.task456_line_follow.params_valid = 1U;
    g_h723_debug.task456_line_follow.params_rejected_count = 0U;
    g_h723_debug.line_follow.pid_kp = APP_H723_LINE_FOLLOW_PID_KP;
    g_h723_debug.line_follow.pid_ki = APP_H723_LINE_FOLLOW_PID_KI;
    g_h723_debug.line_follow.pid_kd = APP_H723_LINE_FOLLOW_PID_KD;
    g_h723_debug.line_follow.pid_output_limit_mm_s =
        APP_H723_LINE_FOLLOW_MAX_TURN_SPEED_MM_S;
    g_h723_debug.line_follow.pid_deadband = APP_H723_LINE_FOLLOW_PID_DEADBAND;
    g_h723_debug.line_follow.reset_pid_request = 0U;
    g_h723_debug.line_follow.params_valid = 1U;
    g_h723_debug.line_follow.params_rejected_count = 0U;
    g_h723_debug.line_follow.active_group = APP_H723_LINE_FOLLOW_GROUP_COMMON;
    g_h723_debug.line_follow.pid_dt_s =
        (float)APP_GRAYSCALE_TASK_PERIOD_MS / 1000.0f;
    g_h723_debug.line_follow.target_position = 0.0f;
    g_h723_debug.line_follow.pid_update_count = 0U;
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
    g_h723_debug.balance.allow_extended_position_range = 0U;
    g_h723_debug.tilt.enable = 0U;
    g_h723_debug.tilt.target_tilt_deg = 0.0f;
    g_h723_debug.tilt.capture_zero_request = 0U;
    g_h723_debug.tilt.pid_kp = APP_H723_TILT_CONTROL_PID_KP;
    g_h723_debug.tilt.pid_ki = APP_H723_TILT_CONTROL_PID_KI;
    g_h723_debug.tilt.pid_kd = APP_H723_TILT_CONTROL_PID_KD;
    g_h723_debug.tilt.derivative_filter_N = APP_H723_TILT_CONTROL_DERIVATIVE_FILTER_N;
    g_h723_debug.tilt.pid_deadband_deg = APP_H723_TILT_CONTROL_PID_DEADBAND_DEG;
    g_h723_debug.tilt.max_position_rate_deg_s = APP_H723_TILT_CONTROL_MAX_POSITION_RATE_DEG_S;
    g_h723_debug.ball_position.enable = 0U;
    g_h723_debug.ball_position.target_mm = 0.0f;
    g_h723_debug.ball_position.pid_kp = s_ball_position_config.pid_params.kp;
    g_h723_debug.ball_position.pid_ki = s_ball_position_config.pid_params.ki;
    g_h723_debug.ball_position.pid_kd = s_ball_position_config.pid_params.kd;
    g_h723_debug.ball_position.output_limit_deg = s_ball_position_config.output_limit_deg;
    g_h723_debug.ball_position.pid_deadband_mm = s_ball_position_config.deadband_mm;
    for (index = 0U; index < APP_BALL_POSITION_HOLD_MAP_POINT_COUNT; ++index) {
        g_h723_debug.ball_position.hold_position_mm[index] =
            s_ball_position_config.hold_position_mm[index];
        g_h723_debug.ball_position.hold_tilt_deg[index] =
            s_ball_position_config.hold_tilt_deg[index];
    }
    g_h723_debug.ball_position.engage_error_mm = s_ball_position_config.engage_error_mm;
    g_h723_debug.ball_position.release_error_mm = s_ball_position_config.release_error_mm;
    g_h723_debug.ball_position.breakaway_positive_tilt_deg =
        s_ball_position_config.breakaway_positive_tilt_deg;
    g_h723_debug.ball_position.breakaway_negative_tilt_deg =
        s_ball_position_config.breakaway_negative_tilt_deg;
    g_h723_debug.ball_position.velocity_gain_deg_per_mm_s =
        s_ball_position_config.velocity_gain_deg_per_mm_s;
    g_h723_debug.ball_position.velocity_filter_alpha =
        s_ball_position_config.velocity_filter_alpha;
    g_h723_debug.ball_position_dynamic.target_mm = 0.0f;
    g_h723_debug.ball_position_dynamic.pid_kp =
        s_ball_position_dynamic_config.pid_params.kp;
    g_h723_debug.ball_position_dynamic.pid_ki =
        s_ball_position_dynamic_config.pid_params.ki;
    g_h723_debug.ball_position_dynamic.pid_kd =
        s_ball_position_dynamic_config.pid_params.kd;
    g_h723_debug.ball_position_dynamic.output_limit_deg =
        s_ball_position_dynamic_config.output_limit_deg;
    g_h723_debug.ball_position_dynamic.pid_deadband_mm =
        s_ball_position_dynamic_config.deadband_mm;
    g_h723_debug.ball_position_dynamic.position_sign =
        s_ball_position_dynamic_config.sign;
    g_h723_debug.ball_position_dynamic.params_valid = 1U;
    g_h723_debug.ball_position_dynamic.params_rejected_count = 0U;
    g_h723_debug.speed_profile.max_speed_mm_s =
        APP_H723_REMOTE_BALL_SPEED_PROFILE_MAX_SPEED_MM_S;
    g_h723_debug.speed_profile.max_accel_mm_s2 =
        APP_H723_REMOTE_BALL_SPEED_PROFILE_MAX_ACCEL_MM_S2;
    g_h723_debug.speed_profile.max_jerk_mm_s3 =
        APP_H723_REMOTE_BALL_SPEED_PROFILE_MAX_JERK_MM_S3;
    g_h723_debug.speed_profile.reset_request = 0U;
    g_h723_debug.speed_profile.params_valid = 1U;
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

/**
 * @brief 将位置跟踪器累计角度换算为车轮行驶里程。
 *
 * 从输出轴角度换算为车轮圆周距离：distance_mm = output_deg * (D * PI) / 360。
 *
 * @param[in] tracker 位置跟踪器指针。
 * @return 行驶里程，单位 mm；tracker 为 NULL 时返回 0.0f。
 */
static float h723_position_tracker_distance_mm(
    const app_m2006_position_tracker_t *tracker)
{
    float output_deg;

    if (tracker == NULL) {
        return 0.0f;
    }
    output_deg = app_m2006_position_tracker_output_degrees(tracker);
    return output_deg * (APP_H723_WHEEL_DIAMETER_MM * APP_H723_PI_F) / 360.0f;
}

/**
 * @brief 计算左右轮行驶里程的平均值。
 *
 * @return 左右轮平均行驶里程，单位 mm。
 */
static float h723_average_distance_mm(void)
{
    return (h723_position_tracker_distance_mm(&s_position_tracker[0]) +
            h723_position_tracker_distance_mm(&s_position_tracker[1])) / 2.0f;
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

#if (APP_H723_BALANCE_ENABLE == 1U) && (APP_H723_TILT_CONTROL_ENABLE == 1U)
static bool h723_tilt_pid_params_are_valid(float kp, float ki, float kd,
                                           float derivative_filter_n, float output_limit,
                                           float deadband)
{
    return isfinite(kp) && isfinite(ki) && isfinite(kd) && isfinite(derivative_filter_n) &&
           isfinite(output_limit) && isfinite(deadband) && kp >= 0.0f && ki >= 0.0f &&
           kd >= 0.0f && derivative_filter_n >= 0.0f && output_limit > 0.0f &&
           deadband >= 0.0f;
}

static void h723_tilt_apply_debug_params(void)
{
    volatile h723_debug_tilt_t *debug = &g_h723_debug.tilt;

    if (h723_tilt_pid_params_are_valid(debug->pid_kp, debug->pid_ki, debug->pid_kd,
                                       debug->derivative_filter_N,
                                       debug->max_position_rate_deg_s,
                                       debug->pid_deadband_deg)) {
        s_tilt_control.config.pid_params.kp = debug->pid_kp;
        s_tilt_control.config.pid_params.ki = debug->pid_ki;
        s_tilt_control.config.pid_params.kd = debug->pid_kd;
        s_tilt_control.config.derivative_filter_N = debug->derivative_filter_N;
        s_tilt_control.config.pid_params.output_limit = debug->max_position_rate_deg_s;
        s_tilt_control.config.pid_params.deadband = debug->pid_deadband_deg;
    }
}

static bool h723_ball_position_params_are_valid(
    const volatile h723_debug_ball_position_t *debug)
{
    uint32_t index;

    if (!isfinite(debug->pid_kp) || !isfinite(debug->pid_ki) ||
        !isfinite(debug->pid_kd) || !isfinite(debug->output_limit_deg) ||
        !isfinite(debug->pid_deadband_mm) || debug->pid_kp < 0.0f ||
        debug->pid_ki < 0.0f || debug->pid_kd < 0.0f ||
        debug->output_limit_deg <= 0.0f || debug->pid_deadband_mm < 0.0f ||
        !isfinite(debug->engage_error_mm) || !isfinite(debug->release_error_mm) ||
        debug->engage_error_mm < debug->release_error_mm ||
        debug->release_error_mm < 0.0f ||
        !isfinite(debug->breakaway_positive_tilt_deg) ||
        !isfinite(debug->breakaway_negative_tilt_deg) ||
        debug->breakaway_positive_tilt_deg < 0.0f ||
        debug->breakaway_negative_tilt_deg < 0.0f ||
        !isfinite(debug->velocity_gain_deg_per_mm_s) ||
        debug->velocity_gain_deg_per_mm_s < 0.0f ||
        !isfinite(debug->velocity_filter_alpha) ||
        debug->velocity_filter_alpha <= 0.0f ||
        debug->velocity_filter_alpha > 1.0f) {
        return false;
    }
    for (index = 0U; index < APP_BALL_POSITION_HOLD_MAP_POINT_COUNT; ++index) {
        if (!isfinite(debug->hold_position_mm[index]) ||
            !isfinite(debug->hold_tilt_deg[index]) ||
            (index > 0U &&
             debug->hold_position_mm[index] <= debug->hold_position_mm[index - 1U])) {
            return false;
        }
    }
    return true;
}

static void h723_ball_position_apply_debug_params(void)
{
    volatile h723_debug_ball_position_t *debug = &g_h723_debug.ball_position;

    uint32_t index;

    if (!h723_ball_position_params_are_valid(debug)) {
        return;
    }
    s_ball_position_config.pid_params.kp = debug->pid_kp;
    s_ball_position_config.pid_params.ki = debug->pid_ki;
    s_ball_position_config.pid_params.kd = debug->pid_kd;
    s_ball_position_config.output_limit_deg = debug->output_limit_deg;
    s_ball_position_config.deadband_mm = debug->pid_deadband_mm;
    for (index = 0U; index < APP_BALL_POSITION_HOLD_MAP_POINT_COUNT; ++index) {
        s_ball_position_config.hold_position_mm[index] = debug->hold_position_mm[index];
        s_ball_position_config.hold_tilt_deg[index] = debug->hold_tilt_deg[index];
    }
    s_ball_position_config.engage_error_mm = debug->engage_error_mm;
    s_ball_position_config.release_error_mm = debug->release_error_mm;
    s_ball_position_config.breakaway_positive_tilt_deg =
        debug->breakaway_positive_tilt_deg;
    s_ball_position_config.breakaway_negative_tilt_deg =
        debug->breakaway_negative_tilt_deg;
    s_ball_position_config.velocity_gain_deg_per_mm_s =
        debug->velocity_gain_deg_per_mm_s;
    s_ball_position_config.velocity_filter_alpha = debug->velocity_filter_alpha;
    s_ball_position_control.config = s_ball_position_config;
    s_ball_position_control.pid.params.kp = debug->pid_kp;
    s_ball_position_control.pid.params.ki = debug->pid_ki;
    s_ball_position_control.pid.params.kd = debug->pid_kd;
    s_ball_position_control.pid.params.output_limit = debug->output_limit_deg;
    s_ball_position_control.pid.params.deadband = debug->pid_deadband_mm;
}

static bool h723_ball_position_dynamic_params_are_valid(
    const volatile h723_debug_ball_position_dynamic_t *debug)
{
    return isfinite(debug->target_mm) && isfinite(debug->pid_kp) &&
           isfinite(debug->pid_ki) && isfinite(debug->pid_kd) &&
           isfinite(debug->output_limit_deg) && isfinite(debug->pid_deadband_mm) &&
           isfinite(debug->position_sign) && debug->pid_kp >= 0.0f &&
           debug->pid_ki >= 0.0f && debug->pid_kd >= 0.0f &&
           debug->output_limit_deg > 0.0f && debug->pid_deadband_mm >= 0.0f &&
           (debug->position_sign == -1.0f || debug->position_sign == 1.0f);
}

static void h723_ball_position_dynamic_apply_debug_params(void)
{
    volatile h723_debug_ball_position_dynamic_t *debug =
        &g_h723_debug.ball_position_dynamic;

    if (!h723_ball_position_dynamic_params_are_valid(debug)) {
        debug->params_valid = 0U;
        debug->params_rejected_count++;
        return;
    }
    debug->params_valid = 1U;
    /* The physical hold/friction calibration remains shared; PID state does not. */
    s_ball_position_dynamic_config = s_ball_position_config;
    s_ball_position_dynamic_config.pid_params.kp = debug->pid_kp;
    s_ball_position_dynamic_config.pid_params.ki = debug->pid_ki;
    s_ball_position_dynamic_config.pid_params.kd = debug->pid_kd;
    s_ball_position_dynamic_config.output_limit_deg = debug->output_limit_deg;
    s_ball_position_dynamic_config.deadband_mm = debug->pid_deadband_mm;
    s_ball_position_dynamic_config.sign = debug->position_sign;
    s_ball_position_dynamic_control.config = s_ball_position_dynamic_config;
    s_ball_position_dynamic_control.pid.params.kp = debug->pid_kp;
    s_ball_position_dynamic_control.pid.params.ki = debug->pid_ki;
    s_ball_position_dynamic_control.pid.params.kd = debug->pid_kd;
    s_ball_position_dynamic_control.pid.params.output_limit = debug->output_limit_deg;
    s_ball_position_dynamic_control.pid.params.deadband = debug->pid_deadband_mm;
}

static bool h723_remote_ball_speed_profile_params_are_valid(
    const volatile h723_debug_speed_profile_t *debug)
{
    return isfinite(debug->max_speed_mm_s) && isfinite(debug->max_accel_mm_s2) &&
           isfinite(debug->max_jerk_mm_s3) && debug->max_speed_mm_s > 0.0f &&
           debug->max_accel_mm_s2 > 0.0f && debug->max_jerk_mm_s3 > 0.0f;
}

static void h723_remote_ball_speed_profile_step(bool active, float requested_speed_mm_s)
{
    volatile h723_debug_speed_profile_t *debug = &g_h723_debug.speed_profile;

    if (!h723_remote_ball_speed_profile_params_are_valid(debug)) {
        debug->params_valid = 0U;
        debug->params_rejected_count++;
        app_speed_profile_reset(&s_remote_ball_speed_profile);
        s_remote_ball_speed_profile.valid = false;
    } else {
        debug->params_valid = 1U;
        s_remote_ball_speed_profile.config.max_speed_mm_s = debug->max_speed_mm_s;
        s_remote_ball_speed_profile.config.max_accel_mm_s2 = debug->max_accel_mm_s2;
        s_remote_ball_speed_profile.config.max_jerk_mm_s3 = debug->max_jerk_mm_s3;
        s_remote_ball_speed_profile.valid = true;
    }
    if (debug->reset_request != 0U) {
        app_speed_profile_reset(&s_remote_ball_speed_profile);
        debug->reset_request = 0U;
    }
    if (active && s_remote_ball_speed_profile.valid) {
        app_speed_profile_step(&s_remote_ball_speed_profile, requested_speed_mm_s,
                               (float)APP_H723_CHASSIS_TASK_PERIOD_MS / 1000.0f,
                               &s_remote_ball_speed_profile_output);
    } else {
        app_speed_profile_reset(&s_remote_ball_speed_profile);
        (void)memset(&s_remote_ball_speed_profile_output, 0,
                     sizeof(s_remote_ball_speed_profile_output));
    }
    debug->active = active ? 1U : 0U;
    debug->requested_speed_mm_s = requested_speed_mm_s;
    debug->planned_speed_mm_s = s_remote_ball_speed_profile_output.planned_speed_mm_s;
    debug->planned_accel_mm_s2 = s_remote_ball_speed_profile_output.planned_accel_mm_s2;
}

#if (APP_H723_BALANCE_ENABLE == 1U)
static void h723_pipe_startup_update(uint32_t now_ms, uint32_t button_mask)
{
    h723_jy901s_control_snapshot_t imu_snapshot = {0};
    const uint32_t id3_index = APP_H723_BALANCE_MOTOR_ID - 1U;
    app_pipe_startup_input_t input = {0};
    uint32_t sample_age_ms = UINT_MAX;
    const app_pipe_startup_state_t previous_state = s_pipe_startup_snapshot.state;
    const bool snapshot_available = h723_jy901s_service_get_snapshot(
        &imu_snapshot, now_ms, &sample_age_ms);

    input.balance_zero_valid = s_balance.zero_valid;
    input.id3_feedback_valid = h723_m2006_feedback_is_fresh(id3_index, now_ms);
    input.id3_position_deg =
        app_m2006_position_tracker_output_degrees(&s_position_tracker[id3_index]) -
        s_balance.zero_offset_deg;
    input.id3_output_speed_rpm = s_feedback[id3_index].output_speed_rpm;
    input.now_ms = now_ms;
    input.imu_pitch_valid = snapshot_available && imu_snapshot.sample_valid &&
                            imu_snapshot.calibration_valid;
    input.imu_pitch_fresh = input.imu_pitch_valid &&
                            sample_age_ms <= APP_H723_TILT_CONTROL_SAMPLE_MAX_AGE_MS;
    input.imu_pitch_deg = imu_snapshot.vehicle_pitch_deg;
    input.buttons = button_mask;
    app_pipe_startup_step(&s_pipe_startup, &input, &s_pipe_startup_snapshot);
    if (previous_state == APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED &&
        s_pipe_startup_snapshot.state == APP_PIPE_STARTUP_STATE_READY) {
        /* Let app_tilt_control consume the one-shot on its next 5 ms sample. */
        g_h723_debug.tilt.capture_zero_request = 1U;
    }
    g_h723_debug.pipe_startup.state = (uint32_t)s_pipe_startup_snapshot.state;
    g_h723_debug.pipe_startup.fault = (uint32_t)s_pipe_startup_snapshot.fault;
    g_h723_debug.pipe_startup.calibration_valid =
        s_pipe_startup_snapshot.calibration_valid ? 1U : 0U;
    g_h723_debug.pipe_startup.id3_allowed =
        s_pipe_startup_snapshot.id3_allowed ? 1U : 0U;
    g_h723_debug.pipe_startup.other_motors_allowed =
        s_pipe_startup_snapshot.other_motors_allowed ? 1U : 0U;
    g_h723_debug.pipe_startup.captured_pitch_deg =
        s_pipe_startup_snapshot.captured_pitch_deg;
    g_h723_debug.pipe_startup.calibration_target_position_deg =
        s_pipe_startup_snapshot.calibration_target_position_deg;
    g_h723_debug.pipe_startup.id3_position_deg = s_pipe_startup_snapshot.id3_position_deg;
    g_h723_debug.pipe_startup.id3_output_speed_rpm =
        s_pipe_startup_snapshot.id3_output_speed_rpm;
    g_h723_debug.pipe_startup.calibration_position_stable =
        s_pipe_startup_snapshot.calibration_position_stable ? 1U : 0U;
}
#else
static void h723_pipe_startup_update(uint32_t now_ms, uint32_t button_mask)
{
    (void)now_ms;
    (void)button_mask;
}
#endif

static void h723_ball_position_publish_debug(const app_k230_sample_t *sample,
                                             uint32_t sample_age_ms,
                                             bool dynamic_profile,
                                             const app_ball_position_control_output_t *output)
{
    volatile h723_debug_ball_position_t *debug = &g_h723_debug.ball_position;

    debug->state = (uint32_t)output->state;
    debug->fault = (uint32_t)output->fault;
    debug->update_due = output->update_due ? 1U : 0U;
    debug->vision_valid = output->valid ? 1U : 0U;
    debug->vision_frame_count = sample != NULL ? sample->valid_frame_count : 0U;
    debug->vision_age_ms = sample_age_ms;
    debug->measured_mm = sample != NULL ? sample->distance_mm : 0.0f;
    debug->error_mm = output->error_mm;
    debug->pid_p_out_deg = output->p_out_deg;
    debug->pid_i_out_deg = output->i_out_deg;
    debug->pid_d_out_deg = output->d_out_deg;
    debug->pid_output_deg = output->output_deg;
    debug->target_tilt_deg = output->target_tilt_deg;
    debug->pid_integral = output->integral;
    debug->drive_active = output->drive_active ? 1U : 0U;
    debug->hold_tilt_output_deg = output->hold_tilt_deg;
    debug->breakaway_tilt_output_deg = output->breakaway_tilt_deg;
    debug->velocity_mm_s = output->velocity_mm_s;
    debug->velocity_damping_tilt_deg = output->velocity_damping_tilt_deg;
    debug->active_profile = dynamic_profile ? 1U : 0U;
}

static void h723_tilt_publish_debug(const app_tilt_control_output_t *output,
                                    uint32_t sample_age_ms)
{
    volatile h723_debug_tilt_t *debug = &g_h723_debug.tilt;

    debug->state = (uint32_t)output->state;
    debug->fault = (uint32_t)output->fault;
    debug->capture_zero_consumed = output->capture_zero_consumed ? 1U : 0U;
    debug->zero_captured_valid = output->zero_captured_valid ? 1U : 0U;
    debug->new_imu_sample = output->new_imu_sample ? 1U : 0U;
    debug->motor_target_clamped = output->motor_target_clamped ? 1U : 0U;
    debug->imu_sample_age_ms = sample_age_ms;
    debug->raw_pitch_deg = output->raw_pitch_deg;
    debug->captured_zero_deg = output->captured_zero_deg;
    debug->tilt_deg = output->tilt_deg;
    debug->error_deg = output->error_deg;
    debug->pid_rate_deg_s = output->pid_rate_deg_s;
    debug->pid_p_out_deg_s = output->pid_p_out_deg_s;
    debug->pid_i_out_deg_s = output->pid_i_out_deg_s;
    debug->pid_d_out_deg_s = output->pid_d_out_deg_s;
    debug->pid_integral = output->pid_integral;
    debug->measured_tilt_rate_deg_s = output->measured_tilt_rate_deg_s;
    debug->filtered_tilt_rate_deg_s = output->filtered_tilt_rate_deg_s;
    debug->motor_target_position_deg = output->motor_target_position_deg;
}

static float h723_tilt_service_step(uint32_t now_ms, uint32_t motor_index)
{
    h723_jy901s_control_snapshot_t imu_snapshot = {0};
    app_k230_sample_t ball_sample = {0};
    app_ball_position_control_input_t ball_input;
    uint32_t sample_age_ms = UINT_MAX;
    uint32_t ball_sample_age_ms = UINT_MAX;
    app_tilt_control_output_t output;
    const bool snapshot_available = h723_jy901s_service_get_snapshot(&imu_snapshot, now_ms,
                                                                        &sample_age_ms);
    const bool ball_snapshot_available = h723_k230_service_get_snapshot(
        &ball_sample, now_ms, &ball_sample_age_ms);
    const bool dynamic_profile = s_command.manual_active &&
                                 s_command.mode ==
                                     APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW_BALL;
    const bool startup_position_move = s_pipe_startup_snapshot.state ==
        APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION;
    const bool ball_enabled = dynamic_profile ||
                              (!dynamic_profile &&
                               g_h723_debug.ball_position.enable != 0U);
    app_ball_position_control_t *ball_control = dynamic_profile ?
        &s_ball_position_dynamic_control : &s_ball_position_control;
    float target_tilt_deg = g_h723_debug.tilt.target_tilt_deg;

    h723_ball_position_apply_debug_params();
    h723_ball_position_dynamic_apply_debug_params();
    if (dynamic_profile != s_ball_position_dynamic_active) {
        app_ball_position_control_reset(&s_ball_position_control);
        app_ball_position_control_reset(&s_ball_position_dynamic_control);
        s_ball_position_dynamic_active = dynamic_profile;
    }
    ball_input.now_ms = now_ms;
    ball_input.enabled = ball_enabled;
    ball_input.target_mm = dynamic_profile ?
        g_h723_debug.ball_position_dynamic.target_mm :
        g_h723_debug.ball_position.target_mm;
    ball_input.measured_mm = ball_sample.distance_mm;
    ball_input.vision_valid = ball_snapshot_available && ball_sample.valid;
    ball_input.vision_age_ms = ball_sample_age_ms;
    ball_input.vision_frame_count = ball_sample.valid_frame_count;
    ball_input.vision_sample_ms = ball_sample.last_frame_ms;
    ball_input.calibration_ready = s_pipe_startup_snapshot.calibration_valid;
    ball_input.id3_ready = s_pipe_startup_snapshot.id3_allowed && s_balance.zero_valid;
    app_ball_position_control_step(ball_control, &ball_input,
                                    &s_ball_position_output);
    if (ball_enabled) {
        target_tilt_deg = s_ball_position_output.target_tilt_deg;
    }
    h723_ball_position_publish_debug(&ball_sample, ball_sample_age_ms, dynamic_profile,
                                     &s_ball_position_output);
    if (startup_position_move) {
        /* Only PC5 may capture the pipe zero after the startup pose is reached. */
        g_h723_debug.tilt.capture_zero_request = 0U;
    }

    const app_tilt_control_input_t input = {
        .now_ms = now_ms,
        .enabled = !startup_position_move &&
                   ((g_h723_debug.tilt.enable != 0U) || ball_enabled),
        .target_tilt_deg = target_tilt_deg,
        .capture_zero_request = !startup_position_move &&
                                g_h723_debug.tilt.capture_zero_request != 0U,
        .balance_zero_valid = s_balance.zero_valid,
        .motor_feedback_valid = h723_m2006_feedback_is_fresh(motor_index, now_ms),
        .motor_feedback_position_deg =
            app_m2006_position_tracker_output_degrees(&s_position_tracker[motor_index]),
        .imu_online = snapshot_available && imu_snapshot.sample_valid,
        .imu_valid = snapshot_available && imu_snapshot.sample_valid &&
                     imu_snapshot.calibration_valid,
        .imu_pitch_deg = imu_snapshot.vehicle_pitch_deg,
        .imu_sample_count = imu_snapshot.complete_sample_count,
        .imu_sample_age_ms = sample_age_ms,
    };

    h723_tilt_apply_debug_params();
    app_tilt_control_step(&s_tilt_control, &input, &output);
    if (output.capture_zero_consumed) {
        g_h723_debug.tilt.capture_zero_request = 0U;
    }
    h723_tilt_publish_debug(&output, sample_age_ms);
    if (startup_position_move) {
        return APP_H723_PIPE_STARTUP_CALIBRATION_POSITION_DEG;
    }
    return input.enabled ? output.motor_target_position_deg :
                           g_h723_debug.balance.target_position_deg;
}
#endif

#if (APP_H723_BALANCE_ENABLE == 1U)
static void h723_balance_service_step(uint32_t now_ms, float output_current_A[3])
{
    const uint32_t index = APP_H723_BALANCE_MOTOR_ID - 1U;
    app_balance_step_input_t input = {
        .now_ms = now_ms,
        .feedback_valid = h723_m2006_feedback_is_fresh(index, now_ms),
        .feedback_position_deg = app_m2006_position_tracker_output_degrees(&s_position_tracker[index]),
        .feedback_output_speed_rpm = s_feedback[index].output_speed_rpm,
        .feedback_current_a = s_feedback[index].current_a,
        .requested_target_position_deg = g_h723_debug.balance.target_position_deg,
        .allow_extended_position_range =
            g_h723_debug.balance.allow_extended_position_range != 0U,
        .rehome_request = g_h723_debug.balance.rehome_request != 0U,
    };
    app_balance_step_output_t result;
    volatile h723_m2006_debug_t *debug = &g_h723_debug.m2006[index];

#if (APP_H723_TILT_CONTROL_ENABLE == 1U)
    input.requested_target_position_deg = h723_tilt_service_step(now_ms, index);
#endif
    if (s_pipe_startup_snapshot.state ==
        APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION) {
        /* The startup pose takes precedence over all Watch and ball-loop requests. */
        input.requested_target_position_deg =
            APP_H723_PIPE_STARTUP_CALIBRATION_POSITION_DEG;
    }
    app_balance_step(&s_balance, &input, &result);
    if (result.rehome_request_consumed) {
        g_h723_debug.balance.rehome_request = 0U;
    }
    g_h723_debug.balance.state = (uint32_t)result.state;
    g_h723_debug.balance.fault = (uint32_t)result.fault;
    g_h723_debug.balance.zero_valid = result.zero_valid ? 1U : 0U;
    g_h723_debug.balance.target_clamped = result.target_clamped ? 1U : 0U;
    g_h723_debug.balance.extended_position_range_active =
        result.extended_position_range_active ? 1U : 0U;
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
    if (!s_pipe_startup_snapshot.id3_allowed &&
        s_pipe_startup_snapshot.state != APP_PIPE_STARTUP_STATE_WAIT_HOME) {
        /* Homing is allowed in WAIT_HOME; calibration and skip never energize ID 3. */
        output_current_A[index] = 0.0f;
        PID_Incremental_Reset(&s_balance.speed_pid);
    }
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
    uint32_t requested_task = 0U;
    bool task2_controls_chassis = false;
    bool task2_was_running = false;
    bool task4_controls_chassis = false;
    bool task4_was_running = false;
    bool task56_controls_chassis = false;
    bool task56_was_running = false;
    bool task_controls_chassis = false;
    while (s_crsf_read_index != s_crsf_write_index) {
        (void)app_crsf_parser_feed(&s_crsf_parser, s_crsf_ring[s_crsf_read_index], now_ms, &s_crsf_input);
        s_crsf_read_index = (uint16_t)((s_crsf_read_index + 1U) % H723_CRSF_RING_SIZE);
    }
    h723_app_buttons_snapshot_copy(&button_snapshot);
    h723_pipe_startup_update(now_ms, button_snapshot.stable_high_mask);
    if (s_pipe_startup_snapshot.state == APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION ||
        s_pipe_startup_snapshot.state == APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED) {
        /* Startup motion and calibration keys must not enter the normal task menu. */
        s_pipe_button_release_required = true;
        button_snapshot.stable_high_mask = 0U;
    } else if (s_pipe_button_release_required) {
        /* Do not turn the capture/skip press into an accidental task confirmation. */
        if (button_snapshot.stable_high_mask == 0U) {
            s_pipe_button_release_required = false;
        }
        button_snapshot.stable_high_mask = 0U;
    }
    app_chassis_control_step(&s_control_state, &s_crsf_input,
                             button_snapshot.stable_high_mask, now_ms,
                             &s_control_output);
    s_command = s_control_output.chassis;
    {
        const bool remote_ball_line_follow = s_command.manual_active &&
            s_command.mode == APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW_BALL;

        h723_remote_ball_speed_profile_step(remote_ball_line_follow,
                                             s_command.base_speed_mm_s);
        if (remote_ball_line_follow) {
            s_command.base_speed_mm_s =
                s_remote_ball_speed_profile_output.planned_speed_mm_s;
        }
    }
    if (s_control_output.remote_takeover) {
        app_task2_abort(&s_task2);
        app_task4_abort(&s_task4);
        app_task56_abort(&s_task56);
        s_line_follow_active_group = APP_H723_LINE_FOLLOW_GROUP_COMMON;
    } else if (app_task_menu_take_execution_request(&requested_task)) {
        if (requested_task == 2U) {
            app_task4_abort(&s_task4);
            app_task2_start(&s_task2, now_ms);
            s_line_follow_active_group = APP_H723_LINE_FOLLOW_GROUP_TASK2;
            app_line_follow_reset(&s_line_follow);
        } else if (requested_task == 4U) {
            app_task2_abort(&s_task2);
            app_task4_start(&s_task4, now_ms, h723_average_distance_mm());
            s_line_follow_active_group = APP_H723_LINE_FOLLOW_GROUP_TASK456;
            app_line_follow_reset(&s_line_follow);
        } else if ((requested_task == 5U) || (requested_task == 6U)) {
            app_task2_abort(&s_task2);
            app_task4_abort(&s_task4);
            app_task56_start(&s_task56, now_ms);
            s_task56_task_id = requested_task;
            s_line_follow_active_group = APP_H723_LINE_FOLLOW_GROUP_TASK456;
            app_line_follow_reset(&s_line_follow);
        } else if (requested_task == 3U) {
            /* Task 3 has no executor: release the menu without issuing motion. */
            app_task2_abort(&s_task2);
            app_task4_abort(&s_task4);
            app_task56_abort(&s_task56);
            s_line_follow_active_group = APP_H723_LINE_FOLLOW_GROUP_COMMON;
            app_line_follow_reset(&s_line_follow);
            app_task_menu_finish_execution();
        }
    }
    task2_was_running = !s_control_output.remote_takeover &&
                        (s_task2.phase == APP_TASK2_PHASE_RUNNING);
    task4_was_running = !s_control_output.remote_takeover &&
                        (s_task4.phase == APP_TASK4_PHASE_RUNNING);
    task56_was_running = !s_control_output.remote_takeover &&
                         (s_task56.phase == APP_TASK56_PHASE_RUNNING);
    if (!s_control_output.remote_takeover &&
        (s_task2.phase == APP_TASK2_PHASE_RUNNING)) {
        const app_task2_input_t task2_input = {
            .now_ms = now_ms,
            .black_count = g_h723_debug.grayscale.black_count,
        };
        app_task2_step(&s_task2, &task2_input);
        app_task2_get_output(&s_task2, &s_task2_output);
        task2_controls_chassis = s_task2_output.follow_line;
        s_command.mode = APP_CHASSIS_MODE_LINE_FOLLOW;
        s_command.manual_active = task2_controls_chassis;
        s_command.base_speed_mm_s = s_task2_output.running ?
            APP_H723_TASK2_SPEED_MM_S : 0.0f;
        if (!task2_controls_chassis) {
            s_command.left_target_rpm = 0.0f;
            s_command.right_target_rpm = 0.0f;
        }
        if (task2_was_running && (s_task2.phase == APP_TASK2_PHASE_STOPPED)) {
            app_task_menu_finish_execution();
        }
    } else if (!s_control_output.remote_takeover &&
               (s_task4.phase == APP_TASK4_PHASE_RUNNING)) {
        const app_task4_input_t task4_input = {
            .now_ms = now_ms,
            .distance_mm = h723_average_distance_mm(),
        };
        app_task4_step(&s_task4, &task4_input);
        app_task4_get_output(&s_task4, &s_task4_output);
        task4_controls_chassis = s_task4_output.follow_line;
        s_command.mode = APP_CHASSIS_MODE_LINE_FOLLOW;
        s_command.manual_active = task4_controls_chassis;
        s_command.base_speed_mm_s = s_task4_output.base_speed_mm_s;
        if (!task4_controls_chassis) {
            s_command.left_target_rpm = 0.0f;
            s_command.right_target_rpm = 0.0f;
        }
        if (task4_was_running && (s_task4.phase == APP_TASK4_PHASE_STOPPED)) {
            app_task_menu_finish_execution();
        }
    } else if (!s_control_output.remote_takeover &&
               (s_task56.phase == APP_TASK56_PHASE_RUNNING)) {
        const app_task56_input_t task56_input = {
            .now_ms = now_ms,
        };
        app_task56_step(&s_task56, &task56_input);
        app_task56_get_output(&s_task56, &s_task56_output);
        task56_controls_chassis = s_task56_output.follow_line;
        s_command.mode = APP_CHASSIS_MODE_LINE_FOLLOW;
        s_command.manual_active = task56_controls_chassis;
        s_command.base_speed_mm_s = s_task56_output.base_speed_mm_s;
        if (!task56_controls_chassis) {
            s_command.left_target_rpm = 0.0f;
            s_command.right_target_rpm = 0.0f;
        }
        if (task56_was_running &&
            (s_task56.phase == APP_TASK56_PHASE_STOPPED)) {
            app_task_menu_finish_execution();
        }
    } else {
        app_task2_get_output(&s_task2, &s_task2_output);
        app_task4_get_output(&s_task4, &s_task4_output);
        app_task56_get_output(&s_task56, &s_task56_output);
    }
    task_controls_chassis = task2_controls_chassis || task4_controls_chassis ||
                            task56_controls_chassis;
    (void)task_controls_chassis;
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
    g_h723_debug.control.buttons_enabled =
        (s_pipe_startup_snapshot.state == APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION ||
         s_pipe_startup_snapshot.state == APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED) ?
        0U : (s_control_output.buttons_enabled ? 1U : 0U);
    g_h723_debug.control.se_pressed = s_control_output.se_pressed ? 1U : 0U;
    g_h723_debug.control.sb_state = s_control_output.sb_state;
    g_h723_debug.control.sc_state = s_control_output.sc_state;
    g_h723_debug.control.button_stable_high_mask = button_snapshot.stable_high_mask;
    g_h723_debug.control.selected_task = s_control_output.selected_task;
    g_h723_debug.control.task_request_available =
        s_control_output.task_request_available ? 1U : 0U;
    if (s_control_output.selected_task == 2U) {
        g_h723_debug.control.active_task_elapsed_ms = s_task2_output.elapsed_ms;
    } else if (s_control_output.selected_task == 4U) {
        g_h723_debug.control.active_task_elapsed_ms = s_task4_output.elapsed_ms;
    } else if ((s_control_output.selected_task == 5U) ||
               (s_control_output.selected_task == 6U)) {
        g_h723_debug.control.active_task_elapsed_ms =
            s_task56_task_id == s_control_output.selected_task ?
            s_task56_output.elapsed_ms : 0U;
    } else {
        g_h723_debug.control.active_task_elapsed_ms = 0U;
    }
    g_h723_debug.task2.phase = (uint32_t)s_task2_output.phase;
    g_h723_debug.task2.fault = 0U;
    g_h723_debug.task2.running = s_task2_output.running ? 1U : 0U;
    g_h723_debug.task2.stop_mark =
        g_h723_debug.grayscale.black_count >= APP_H723_TASK2_STOP_BLACK_COUNT ? 1U : 0U;
    g_h723_debug.task2.elapsed_ms = s_task2_output.elapsed_ms;
    g_h723_debug.task2.distance_mm = 0.0f;
    g_h723_debug.task2.base_speed_mm_s = s_task2_output.base_speed_mm_s;
    g_h723_debug.task4.phase = (uint32_t)s_task4_output.phase;
    g_h723_debug.task4.running = s_task4_output.running ? 1U : 0U;
    g_h723_debug.task4.elapsed_ms = s_task4_output.elapsed_ms;
    g_h723_debug.task4.distance_mm = s_task4_output.distance_mm;
    g_h723_debug.task4.base_speed_mm_s = s_task4_output.base_speed_mm_s;
    g_h723_debug.task56.task_id = s_task56_task_id;
    g_h723_debug.task56.phase = (uint32_t)s_task56_output.phase;
    g_h723_debug.task56.running = s_task56_output.running ? 1U : 0U;
    g_h723_debug.task56.elapsed_ms = s_task56_output.elapsed_ms;
    g_h723_debug.task56.base_speed_mm_s = s_task56_output.base_speed_mm_s;
    if (task2_controls_chassis) {
        s_line_follow_active_group = APP_H723_LINE_FOLLOW_GROUP_TASK2;
    } else if (task4_controls_chassis || task56_controls_chassis) {
        s_line_follow_active_group = APP_H723_LINE_FOLLOW_GROUP_TASK456;
    } else {
        s_line_follow_active_group = APP_H723_LINE_FOLLOW_GROUP_COMMON;
    }
    h723_line_follow_apply_debug_params(s_line_follow_active_group);
#if (APP_H723_SINGLE_MOTOR_PID_DEBUG_ENABLE == 1U)
    g_h723_debug.chassis.mode = 2U;
    g_h723_debug.chassis.actuation_enabled = g_h723_debug.single_motor.enable;
    g_h723_debug.chassis.forward_normalized = 0.0f;
    g_h723_debug.chassis.turn_normalized = 0.0f;
    g_h723_debug.chassis.left_target_output_speed_rpm = 0.0f;
    g_h723_debug.chassis.right_target_output_speed_rpm = 0.0f;
    h723_single_motor_service_step(now_ms, output_current_A);
#else
    if ((s_command.mode == APP_CHASSIS_MODE_LINE_FOLLOW ||
         s_command.mode == APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW_BALL) &&
        s_command.manual_active) {
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
            if (task_controls_chassis) {
                s_command.manual_active = false;
            }
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
            ((s_command.mode == APP_CHASSIS_MODE_LINE_FOLLOW ||
              s_command.mode == APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW_BALL) &&
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
    h723_line_follow_publish_debug();
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
