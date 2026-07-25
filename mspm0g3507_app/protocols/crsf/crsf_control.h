#ifndef CRSF_CONTROL_H
#define CRSF_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "drivers/motor/board_motor.h"
#include "config/crsf_config.h"
#include "protocols/crsf/crsf_protocol.h"

typedef struct {
    bool valid;
    uint16_t channels[CRSF_CHANNEL_COUNT];
    uint32_t last_valid_time_ms;
} crsf_control_input_t;

typedef enum {
    CRSF_DRIVE_MODE_IDLE = 0U,
    CRSF_DRIVE_MODE_MANUAL,
    CRSF_DRIVE_MODE_LINE_TRACKING,
} crsf_drive_mode_t;

/**
 * @brief 根据链路状态和 CH5 SB 三档值选择底盘模式。
 *
 * 无效、超时或超出标准 CRSF 范围的开关值返回空闲模式。
 */
crsf_drive_mode_t crsf_control_get_drive_mode(
    const crsf_control_input_t *input, uint32_t now_ms);

/**
 * @brief 读取经过死区处理的 CH3 基础速度。
 *
 * @param[out] speed_mm_per_s
 *     CH3 映射得到的带符号基础速度，单位为 mm/s。
 * @return 链路有效且未超时时返回 true，否则返回 false 并输出 0。
 */
bool crsf_control_get_forward_speed(const crsf_control_input_t *input,
                                    uint32_t now_ms,
                                    float *speed_mm_per_s);

bool crsf_control_mix(const crsf_control_input_t *input,
                      uint32_t now_ms,
                      float targets[BOARD_MOTOR_COUNT]);

#endif
