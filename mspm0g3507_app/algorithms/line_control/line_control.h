#ifndef LINE_CONTROL_H
#define LINE_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "pid.h"
#include "drivers/motor/board_motor.h"

typedef struct {
    int32_t line_error;
    uint32_t line_strength;
    uint8_t adc_timeout_mask;
    uint32_t sequence;
    float base_speed_mm_per_s;
    uint32_t now_ms;
} line_control_input_t;

typedef struct {
    bool line_valid;
    uint32_t lost_ms;
    float turn_speed_mm_per_s;
    float pid_p_out;
    float pid_i_out;
    float pid_d_out;
    float pid_output;
    float wheel_targets_mm_per_s[BOARD_MOTOR_COUNT];
} line_control_output_t;

typedef struct {
    bool use_pid_override;
    PID_Position_Param_Config pid_params;
    float max_turn_speed_mm_per_s;
    float max_wheel_speed_mm_per_s;
    float turn_sign;
    uint32_t line_strength_enter;
    uint32_t line_strength_exit;
    uint32_t lost_line_timeout_ms;
    uint32_t line_error_filter_time_constant_ms;
} line_control_debug_t;

typedef struct {
    PID_Position pid;
    bool has_valid_line;
    bool line_valid;
    bool has_filtered_line_error;
    bool has_position_update;
    uint32_t last_sequence;
    uint32_t last_valid_time_ms;
    uint32_t last_filter_time_ms;
    uint32_t last_position_update_ms;
    float filtered_line_error;
    float last_turn_speed_mm_per_s;
} line_control_state_t;

extern volatile line_control_debug_t g_line_control_debug;

/**
 * @brief 初始化线控位置式 PID 状态。
 *
 * @param[out] state
 *     线控状态。后续必须由同一个控制任务独占使用。
 */
void line_control_init(line_control_state_t *state);

/**
 * @brief 清除线控 PID、采样序列和丢线状态。
 *
 * @param[in,out] state
 *     已初始化的线控状态。
 */
void line_control_reset(line_control_state_t *state);

/**
 * @brief 根据灰度位置误差生成四轮速度目标。
 *
 * 每个新的灰度 sequence 最多更新一次位置式 PID。丢线时冻结 PID，
 * 在配置的超时窗口内沿用上次转向输出，超时后返回四轮零目标。
 *
 * @param[in,out] state
 *     线控算法状态，只能在控制任务上下文访问。
 * @param[in] input
 *     灰度观察、基础速度和当前时间，速度单位为 mm/s，时间单位为 ms。
 * @param[out] output
 *     线控状态和四轮速度目标。
 */
void line_control_step(line_control_state_t *state,
                       const line_control_input_t *input,
                       line_control_output_t *output);

#endif
