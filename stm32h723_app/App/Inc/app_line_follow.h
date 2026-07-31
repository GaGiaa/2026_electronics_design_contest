#ifndef APP_LINE_FOLLOW_H
#define APP_LINE_FOLLOW_H

#include <stdbool.h>
#include <stdint.h>

#include "pid.h"

/**
 * @brief 灰度巡线控制器的一次输入快照。
 *
 * `line_position` 的正负方向沿用灰度模块定义。每个新的 `sequence` 最多
 * 更新一次位置 PID；重复快照仍复用最近一次转向修正值。
 */
typedef struct {
    float line_position;
    uint32_t line_strength;
    uint8_t adc_timeout_mask;
    uint32_t sequence;
    float base_speed_mm_s;
} app_line_follow_input_t;

/**
 * @brief 灰度巡线控制器的输出快照。
 *
 * 左右目标速度已经转换为 M2006 输出轴 RPM 并应用底盘方向符号。
 */
typedef struct {
    bool active;
    bool line_valid;
    float line_position;
    float turn_correction_mm_s;
    float left_target_rpm;
    float right_target_rpm;
    uint32_t sequence;
} app_line_follow_output_t;

/**
 * @brief 灰度巡线控制器状态。
 *
 * 调用 `app_line_follow_step()` 前必须通过 `app_line_follow_init()` 初始化。
 * 此对象只允许由单个底盘任务访问。
 */
typedef struct {
    PID_Position pid;
    uint32_t last_sequence;
    bool has_sequence;
    float turn_correction_mm_s;
} app_line_follow_state_t;

/**
 * @brief 初始化灰度巡线位置 PID。
 *
 * @param[out] state 巡线状态对象。
 * @param[in] params 位置 PID 参数，输出单位为 mm/s。
 * @param[in] dt_s 灰度发布周期，单位 s。
 */
void app_line_follow_init(app_line_follow_state_t *state,
                          const PID_Position_Param_Config *params,
                          float dt_s);

/**
 * @brief 清除巡线 PID 与巡线状态。
 *
 * @param[in,out] state 巡线状态对象。
 */
void app_line_follow_reset(app_line_follow_state_t *state);

/**
 * @brief 根据灰度快照生成左右轮目标速度。
 *
 * ADC 超时、线强度不足或无快照时输出非活动的零速度。
 *
 * @param[in,out] state 巡线状态对象。
 * @param[in] input 灰度输入快照。
 * @param[out] output 计算结果。
 */
void app_line_follow_step(app_line_follow_state_t *state,
                          const app_line_follow_input_t *input,
                          app_line_follow_output_t *output);

#endif
