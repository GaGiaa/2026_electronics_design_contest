#ifndef IMU_FUSION_H
#define IMU_FUSION_H

#include <stdbool.h>
#include <stdint.h>

#include "drivers/imu/board_bmi160.h"

#define IMU_FUSION_STARTUP_CALIBRATION_TIME_S 1.0f
#define IMU_FUSION_STATIONARY_CONFIRM_TIME_S 0.20f
#define IMU_FUSION_STATIONARY_GYRO_RATE_MAX_DPS 0.5f
#define IMU_FUSION_CALIBRATION_GYRO_RATE_MAX_DPS 2.5f
#define IMU_FUSION_GYRO_RATE_DEADBAND_DPS 0.30f
#define IMU_FUSION_CALIBRATION_INITIAL_SAMPLES 20U
#define IMU_FUSION_CALIBRATION_OUTLIER_MARGIN_DPS 0.50f
#define IMU_FUSION_ACCEL_NORM_MIN_G 0.75f
#define IMU_FUSION_ACCEL_NORM_MAX_G 1.25f
#define IMU_FUSION_BIAS_TIME_CONSTANT_S 5.0f
#define IMU_FUSION_MAHONY_KP 2.0f
#define IMU_FUSION_MAHONY_KI 0.05f

typedef struct {
    float quaternion[4];
    float yaw_deg;
    float yaw_rate_dps;
    float roll_deg;
    float pitch_deg;
    float accel_norm_g;
    float gyro_bias_x_dps;
    float gyro_bias_y_dps;
    float gyro_bias_z_dps;
    float dt_s;
    float calibration_m2_dps2[3];
    float calibration_elapsed_s;
    float stationary_elapsed_s;
    float gyro_integral[3];
    float previous_yaw_deg;
    uint32_t calibration_samples;
    bool calibrated;
    bool stationary_confirmed;
    bool acceleration_valid;
} imu_fusion_state_t;

/**
 * @brief 初始化纯六轴 IMU 融合状态。
 *
 * 初始化后必须在车辆静止时持续提供有效 BMI160 样本，直到完成约一秒的
 * 三轴陀螺仪零偏校准。初始化会将相对 yaw 设为 0 度。
 *
 * @param[out] state
 *     融合状态对象，不能为 NULL。
 */
void imu_fusion_init(imu_fusion_state_t *state);

/**
 * @brief 请求下一次更新重新进行静止校准。
 *
 * 请求由下次 imu_fusion_update() 消费。消费后状态保持未校准，调用者应在
 * 车辆静止期间继续提供样本；校准完成前不应使用 yaw 输出。
 */
void imu_fusion_request_recalibration(void);

/**
 * @brief 使用一个 BMI160 六轴样本更新姿态和相对 yaw。
 *
 * 加速度模长接近 1 g 时用于 roll/pitch 重力校正，不能提供绝对 yaw；当
 * 加速度受到冲击或线性加速度污染时，暂时回退到陀螺仪积分。
 *
 * @param[in,out] state
 *     已初始化的融合状态对象。
 * @param[in] imu_sample
 *     BMI160 原始加速度和陀螺仪样本。
 * @param[in] dt_s
 *     当前样本与上一个成功样本之间的实际时间，单位为秒，必须大于 0。
 */
void imu_fusion_update(imu_fusion_state_t *state,
                       const board_bmi160_sample_t *imu_sample,
                       float dt_s);

#endif
