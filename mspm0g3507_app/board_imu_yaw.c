#include "board_imu_yaw.h"

#include <math.h>
#include <stddef.h>

#define BOARD_IMU_YAW_PI 3.14159265358979323846f
#define BOARD_IMU_YAW_GYRO_LSB_PER_DPS 65.6f
#define BOARD_IMU_YAW_ACCEL_LSB_PER_G 8192.0f
#define BOARD_IMU_YAW_GYRO_Z_SIGN 1.0f
#define BOARD_IMU_YAW_GYRO_WEIGHT 0.98f
#define BOARD_IMU_YAW_ENCODER_WEIGHT 0.02f
#define BOARD_IMU_YAW_STATIONARY_SPEED_MM_PER_S 20.0f
#define BOARD_IMU_YAW_ACCEL_NORM_MIN_G 0.80f
#define BOARD_IMU_YAW_ACCEL_NORM_MAX_G 1.20f
#define BOARD_IMU_YAW_BIAS_TIME_CONSTANT_S 10.0f

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
                                  const board_encoder_sample_t encoder_samples[BOARD_MOTOR_COUNT])
{
    float left_speed = (encoder_samples[BOARD_MOTOR_FRONT_LEFT].speed_mm_per_s +
                        encoder_samples[BOARD_MOTOR_REAR_LEFT].speed_mm_per_s) * 0.5f;
    float right_speed = (encoder_samples[BOARD_MOTOR_FRONT_RIGHT].speed_mm_per_s +
                         encoder_samples[BOARD_MOTOR_REAR_RIGHT].speed_mm_per_s) * 0.5f;

    return (fabsf(left_speed) <= BOARD_IMU_YAW_STATIONARY_SPEED_MM_PER_S) &&
           (fabsf(right_speed) <= BOARD_IMU_YAW_STATIONARY_SPEED_MM_PER_S) &&
           acceleration_is_level(imu_sample);
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

void board_imu_yaw_init(board_imu_yaw_state_t *state, float track_width_mm)
{
    if (state == NULL) {
        return;
    }

    state->yaw_deg = 0.0f;
    state->yaw_rate_dps = 0.0f;
    state->gyro_bias_z_dps = 0.0f;
    state->track_width_mm = track_width_mm;
    state->gyro_bias_sum_dps = 0.0f;
    state->calibration_samples = 0U;
    state->calibrated = false;
}

void board_imu_yaw_update(board_imu_yaw_state_t *state,
                           const board_bmi160_sample_t *imu_sample,
                           const board_encoder_sample_t encoder_samples[BOARD_MOTOR_COUNT],
                           float dt_s)
{
    float gyro_z_dps;
    float encoder_rate_dps;
    bool stationary;

    if ((state == NULL) || (imu_sample == NULL) || (encoder_samples == NULL) ||
        (dt_s <= 0.0f)) {
        return;
    }

    gyro_z_dps = gyro_z_to_dps(imu_sample->gyro_z);
    stationary = vehicle_is_stationary(imu_sample, encoder_samples);

    if (!state->calibrated) {
        if (stationary) {
            state->gyro_bias_sum_dps += gyro_z_dps;
            ++state->calibration_samples;
            state->gyro_bias_z_dps = state->gyro_bias_sum_dps /
                                     (float)state->calibration_samples;
            if (state->calibration_samples >=
                BOARD_IMU_YAW_STARTUP_CALIBRATION_SAMPLES) {
                state->calibrated = true;
            }
        }
        state->yaw_rate_dps = 0.0f;
        return;
    }

    if (stationary) {
        float bias_alpha = dt_s / BOARD_IMU_YAW_BIAS_TIME_CONSTANT_S;
        if (bias_alpha > 1.0f) {
            bias_alpha = 1.0f;
        }
        state->gyro_bias_z_dps +=
            bias_alpha * (gyro_z_dps - state->gyro_bias_z_dps);
    }

    encoder_rate_dps = encoder_yaw_rate_dps(state, encoder_samples);
    state->yaw_rate_dps =
        (BOARD_IMU_YAW_GYRO_WEIGHT * (gyro_z_dps - state->gyro_bias_z_dps)) +
        (BOARD_IMU_YAW_ENCODER_WEIGHT * encoder_rate_dps);
    state->yaw_deg += state->yaw_rate_dps * dt_s;
    normalize_yaw(&state->yaw_deg);
}
