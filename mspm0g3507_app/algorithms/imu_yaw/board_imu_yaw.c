#include "board_imu_yaw.h"

#include <math.h>
#include <stddef.h>

/* 单位换算 */
/* 圆周率，用于将角速度从弧度每秒换算为角度每秒。 */
#define BOARD_IMU_YAW_PI 3.14159265358979323846f
/* BMI160 在 +/-500 dps 量程下的陀螺仪灵敏度，单位为 LSB/dps。 */
#define BOARD_IMU_YAW_GYRO_LSB_PER_DPS 65.6f
/* BMI160 在 +/-4 g 量程下的加速度灵敏度，单位为 LSB/g。 */
#define BOARD_IMU_YAW_ACCEL_LSB_PER_G 8192.0f

/* 传感器方向 */
/* 陀螺仪 Z 轴符号，按安装方向决定，+1 表示无需反向。 */
#define BOARD_IMU_YAW_GYRO_Z_SIGN 1.0f

/* 融合权重 */
/* 陀螺仪角速度在融合结果中的权重。 */
#define BOARD_IMU_YAW_GYRO_WEIGHT 0.98f
/* 左右轮差速里程计角速度在融合结果中的权重。 */
#define BOARD_IMU_YAW_ENCODER_WEIGHT 0.02f

/* 静止判定与零偏跟踪 */
/* 左右轮速度绝对值均低于该阈值时，允许判定车辆处于静止状态。 */
#define BOARD_IMU_YAW_STATIONARY_SPEED_MM_PER_S 20.0f
/* 静止判定时允许的加速度模长下限，单位为 g。 */
#define BOARD_IMU_YAW_ACCEL_NORM_MIN_G 0.80f
/* 静止判定时允许的加速度模长上限，单位为 g。 */
#define BOARD_IMU_YAW_ACCEL_NORM_MAX_G 1.20f
/* 静止状态下零偏指数跟踪的时间常数，单位为秒。 */
#define BOARD_IMU_YAW_BIAS_TIME_CONSTANT_S 5.0f

static volatile bool g_recalibration_requested;

static float gyro_z_to_dps(int16_t raw_value)
{
    /* Default vehicle frame: +X forward, +Y left, +Z up. */
    return BOARD_IMU_YAW_GYRO_Z_SIGN * (float)raw_value /
           BOARD_IMU_YAW_GYRO_LSB_PER_DPS;
}

static bool acceleration_is_level(const board_bmi160_sample_t *sample)
{
    float accel_x = (float)sample->accel_x / BOARD_IMU_YAW_ACCEL_LSB_PER_G;
    float accel_y = (float)sample->accel_y / BOARD_IMU_YAW_ACCEL_LSB_PER_G;
    float accel_z = (float)sample->accel_z / BOARD_IMU_YAW_ACCEL_LSB_PER_G;
    float norm_squared = (accel_x * accel_x) + (accel_y * accel_y) +
                         (accel_z * accel_z);

    return (norm_squared >= (BOARD_IMU_YAW_ACCEL_NORM_MIN_G *
                             BOARD_IMU_YAW_ACCEL_NORM_MIN_G)) &&
           (norm_squared <= (BOARD_IMU_YAW_ACCEL_NORM_MAX_G *
                             BOARD_IMU_YAW_ACCEL_NORM_MAX_G));
}

static bool vehicle_is_stationary(const board_bmi160_sample_t *imu_sample,
                                  const board_encoder_sample_t encoder_samples[BOARD_MOTOR_COUNT],
                                  float gyro_z_dps,
                                  float gyro_bias_z_dps,
                                  bool calibrated)
{
    float left_speed = (encoder_samples[BOARD_MOTOR_FRONT_LEFT].speed_mm_per_s +
                        encoder_samples[BOARD_MOTOR_REAR_LEFT].speed_mm_per_s) * 0.5f;
    float right_speed = (encoder_samples[BOARD_MOTOR_FRONT_RIGHT].speed_mm_per_s +
                         encoder_samples[BOARD_MOTOR_REAR_RIGHT].speed_mm_per_s) * 0.5f;

    float gyro_rate_for_stationary = calibrated ?
        (gyro_z_dps - gyro_bias_z_dps) : gyro_z_dps;
    float gyro_rate_limit = calibrated ?
        BOARD_IMU_YAW_STATIONARY_GYRO_RATE_MAX_DPS :
        BOARD_IMU_YAW_CALIBRATION_GYRO_RATE_MAX_DPS;

    return (fabsf(left_speed) <= BOARD_IMU_YAW_STATIONARY_SPEED_MM_PER_S) &&
           (fabsf(right_speed) <= BOARD_IMU_YAW_STATIONARY_SPEED_MM_PER_S) &&
           acceleration_is_level(imu_sample) &&
           (fabsf(gyro_rate_for_stationary) <= gyro_rate_limit);
}

static float encoder_yaw_rate_dps(const board_imu_yaw_state_t *state,
                                  const board_encoder_sample_t encoder_samples[BOARD_MOTOR_COUNT])
{
    float left_speed;
    float right_speed;
    float yaw_rate_rad_per_s;

    if (state->track_width_mm <= 0.0f) {
        return 0.0f;
    }

    left_speed = (encoder_samples[BOARD_MOTOR_FRONT_LEFT].speed_mm_per_s +
                  encoder_samples[BOARD_MOTOR_REAR_LEFT].speed_mm_per_s) * 0.5f;
    right_speed = (encoder_samples[BOARD_MOTOR_FRONT_RIGHT].speed_mm_per_s +
                   encoder_samples[BOARD_MOTOR_REAR_RIGHT].speed_mm_per_s) * 0.5f;
    yaw_rate_rad_per_s = (right_speed - left_speed) / state->track_width_mm;
    return yaw_rate_rad_per_s * (180.0f / BOARD_IMU_YAW_PI);
}

static void normalize_yaw(float *yaw_deg)
{
    while (*yaw_deg >= 180.0f) {
        *yaw_deg -= 360.0f;
    }
    while (*yaw_deg < -180.0f) {
        *yaw_deg += 360.0f;
    }
}

static bool calibration_sample_is_valid(const board_imu_yaw_state_t *state,
                                        float gyro_z_dps)
{
    float deviation_limit_dps;

    if (state->calibration_samples < BOARD_IMU_YAW_CALIBRATION_INITIAL_SAMPLES) {
        return true;
    }
    deviation_limit_dps = BOARD_IMU_YAW_CALIBRATION_OUTLIER_MARGIN_DPS;
    if (state->gyro_bias_variance_dps2 > 0.0f) {
        float sigma_limit_dps = 4.0f * sqrtf(state->gyro_bias_variance_dps2);

        if (sigma_limit_dps > deviation_limit_dps) {
            deviation_limit_dps = sigma_limit_dps;
        }
    }
    return fabsf(gyro_z_dps - state->gyro_bias_z_dps) <= deviation_limit_dps;
}

static void accept_calibration_sample(board_imu_yaw_state_t *state,
                                      float gyro_z_dps)
{
    float sample_count = (float)(state->calibration_samples + 1U);
    float delta_dps = gyro_z_dps - state->gyro_bias_z_dps;
    float delta_after_mean_dps;

    state->gyro_bias_z_dps += delta_dps / sample_count;
    delta_after_mean_dps = gyro_z_dps - state->gyro_bias_z_dps;
    state->gyro_calibration_m2_dps2 += delta_dps * delta_after_mean_dps;
    state->gyro_bias_sum_dps += gyro_z_dps;
    ++state->calibration_samples;
    state->gyro_bias_variance_dps2 = state->calibration_samples > 1U ?
        state->gyro_calibration_m2_dps2 /
            (float)(state->calibration_samples - 1U) : 0.0f;
}

static void reset_calibration_state(board_imu_yaw_state_t *state)
{
    state->yaw_deg = 0.0f;
    state->yaw_rate_dps = 0.0f;
    state->gyro_bias_z_dps = 0.0f;
    state->gyro_bias_sum_dps = 0.0f;
    state->gyro_bias_variance_dps2 = 0.0f;
    state->gyro_rate_filtered_dps = 0.0f;
    state->gyro_calibration_m2_dps2 = 0.0f;
    state->calibration_samples = 0U;
    state->stationary_samples = 0U;
    state->calibrated = false;
    state->stationary_confirmed = false;
}

void board_imu_yaw_init(board_imu_yaw_state_t *state, float track_width_mm)
{
    if (state == NULL) {
        return;
    }

    state->track_width_mm = track_width_mm;
    reset_calibration_state(state);
    g_recalibration_requested = false;
}

void board_imu_yaw_request_recalibration(void)
{
    g_recalibration_requested = true;
}

void board_imu_yaw_update(board_imu_yaw_state_t *state,
                           const board_bmi160_sample_t *imu_sample,
                           const board_encoder_sample_t encoder_samples[BOARD_MOTOR_COUNT],
                           float dt_s)
{
    float gyro_z_dps;
    float corrected_gyro_z_dps;
    float encoder_rate_dps;
    bool stationary;
    bool stationary_confirmed;

    if ((state == NULL) || (imu_sample == NULL) || (encoder_samples == NULL) ||
        (dt_s <= 0.0f)) {
        return;
    }

    if (g_recalibration_requested) {
        reset_calibration_state(state);
        g_recalibration_requested = false;
    }

    gyro_z_dps = gyro_z_to_dps(imu_sample->gyro_z);
    stationary = vehicle_is_stationary(imu_sample, encoder_samples, gyro_z_dps,
                                       state->gyro_bias_z_dps,
                                       state->calibrated);
    if (stationary) {
        if (state->stationary_samples < BOARD_IMU_YAW_STATIONARY_CONFIRM_SAMPLES) {
            ++state->stationary_samples;
        }
    } else {
        state->stationary_samples = 0U;
    }
    stationary_confirmed =
        state->stationary_samples >= BOARD_IMU_YAW_STATIONARY_CONFIRM_SAMPLES;

    if (!state->calibrated) {
        if (stationary && calibration_sample_is_valid(state, gyro_z_dps)) {
            accept_calibration_sample(state, gyro_z_dps);
            if (state->calibration_samples >=
                BOARD_IMU_YAW_STARTUP_CALIBRATION_SAMPLES) {
                state->calibrated = true;
            }
        }
        state->stationary_confirmed = false;
        state->gyro_rate_filtered_dps = 0.0f;
        state->yaw_rate_dps = 0.0f;
        return;
    }

    if (stationary_confirmed) {
        float bias_alpha = dt_s / BOARD_IMU_YAW_BIAS_TIME_CONSTANT_S;
        if (bias_alpha > 1.0f) {
            bias_alpha = 1.0f;
        }
        state->gyro_bias_z_dps +=
            bias_alpha * (gyro_z_dps - state->gyro_bias_z_dps);

        state->stationary_confirmed = true;
        state->gyro_rate_filtered_dps = 0.0f;

        /* 静止后保持当前航向，避免残余零偏继续被积分。 */
        state->yaw_rate_dps = 0.0f;
        return;
    }

    state->stationary_confirmed = false;
    encoder_rate_dps = encoder_yaw_rate_dps(state, encoder_samples);
    corrected_gyro_z_dps = gyro_z_dps - state->gyro_bias_z_dps;
    if (fabsf(corrected_gyro_z_dps) <= BOARD_IMU_YAW_GYRO_RATE_DEADBAND_DPS) {
        corrected_gyro_z_dps = 0.0f;
    }
    state->gyro_rate_filtered_dps = corrected_gyro_z_dps;
    state->yaw_rate_dps =
        (BOARD_IMU_YAW_GYRO_WEIGHT * corrected_gyro_z_dps) +
        (BOARD_IMU_YAW_ENCODER_WEIGHT * encoder_rate_dps);
    state->yaw_deg += state->yaw_rate_dps * dt_s;
    normalize_yaw(&state->yaw_deg);
}
