#ifndef YAW_CONTROL_H
#define YAW_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "algorithms/pid/pid.h"
#include "drivers/motor/board_motor.h"

typedef struct {
    float feedback_yaw_deg;
    bool feedback_valid;
    float base_speed_mm_per_s;
    uint32_t now_ms;
} yaw_control_input_t;

typedef struct {
    bool yaw_valid;
    float yaw_error_deg;
    float turn_speed_mm_per_s;
    float pid_p_out;
    float pid_i_out;
    float pid_d_out;
    float pid_output;
    float wheel_targets_mm_per_s[BOARD_MOTOR_COUNT];
} yaw_control_output_t;

typedef struct {
    bool use_pid_override;
    float target_yaw_deg;
    PID_Position_Param_Config pid_params;
    float max_turn_speed_mm_per_s;
    float max_wheel_speed_mm_per_s;
    float turn_sign;
} yaw_control_debug_t;

typedef struct {
    PID_Position pid;
    bool has_position_update;
    bool has_last_yaw_error;
    uint32_t last_position_update_ms;
    float last_yaw_error_deg;
    float last_turn_speed_mm_per_s;
} yaw_control_state_t;

extern volatile yaw_control_debug_t g_yaw_control_debug;

/**
 * @brief 初始化 yaw 锁定位置环状态。
 *
 * @param[out] state
 *     yaw 控制状态。后续必须由同一个控制任务独占使用。
 */
void yaw_control_init(yaw_control_state_t *state);

/**
 * @brief 清除 yaw 锁定位置环和转向输出。
 *
 * @param[in,out] state
 *     已初始化的 yaw 控制状态。
 */
void yaw_control_reset(yaw_control_state_t *state);

/**
 * @brief 根据 IMU yaw 和基础速度生成四轮速度目标。
 *
 * yaw 位置环首次获得有效样本时立即更新，后续按照
 * `APP_YAW_CONTROL_INTERVAL_MS` 更新。未到更新时间时保持上次转向速度，
 * 但仍使用当前基础速度重新混控。角度误差按最短路径归一化到
 * `[-180, 180)`；IMU 无效或基础速度非法时清零目标并复位状态。
 *
 * @param[in,out] state
 *     yaw 控制状态，只能在控制任务上下文访问。
 * @param[in] input
 *     IMU yaw、有效标志、基础速度和当前时间。角度单位为 deg，速度单位为 mm/s，
 *     时间单位为 ms。
 * @param[out] output
 *     yaw 有效状态、误差、PID 输出、转向速度和四轮速度目标。
 */
void yaw_control_step(yaw_control_state_t *state,
                      const yaw_control_input_t *input,
                      yaw_control_output_t *output);

#endif
