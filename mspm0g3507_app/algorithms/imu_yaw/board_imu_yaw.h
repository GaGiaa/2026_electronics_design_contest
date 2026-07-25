#ifndef BOARD_IMU_YAW_H
#define BOARD_IMU_YAW_H

#include <stdbool.h>
#include <stdint.h>

#include "drivers/imu/board_bmi160.h"
#include "drivers/encoder/board_encoder.h"

/* 启动阶段用于计算陀螺仪 Z 轴零偏的静止样本数。 */
#define BOARD_IMU_YAW_STARTUP_CALIBRATION_SAMPLES 100U
/* 连续满足静止条件的样本数，达到后锁定 yaw 输出。 */
#define BOARD_IMU_YAW_STATIONARY_CONFIRM_SAMPLES 5U

typedef struct {
    float yaw_deg;
    float yaw_rate_dps;
    float gyro_bias_z_dps;
    float track_width_mm;
    float gyro_bias_sum_dps;
    uint32_t calibration_samples;
    uint32_t stationary_samples;
    bool calibrated;
} board_imu_yaw_state_t;

void board_imu_yaw_init(board_imu_yaw_state_t *state, float track_width_mm);
void board_imu_yaw_update(board_imu_yaw_state_t *state,
                           const board_bmi160_sample_t *imu_sample,
                           const board_encoder_sample_t encoder_samples[BOARD_MOTOR_COUNT],
                           float dt_s);

#endif
