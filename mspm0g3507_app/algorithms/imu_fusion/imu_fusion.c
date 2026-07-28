#include "imu_fusion.h"

#include <math.h>
#include <stddef.h>

#define IMU_FUSION_PI 3.14159265358979323846f
#define IMU_FUSION_DEG_TO_RAD (IMU_FUSION_PI / 180.0f)
#define IMU_FUSION_RAD_TO_DEG (180.0f / IMU_FUSION_PI)
#define IMU_FUSION_GYRO_LSB_PER_DPS 65.6f
#define IMU_FUSION_ACCEL_LSB_PER_G 8192.0f
#define IMU_FUSION_GYRO_Z_SIGN 1.0f
#define IMU_FUSION_QUATERNION_MIN_NORM 0.000001f

static volatile bool g_recalibration_requested;

static float clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static float normalize_angle_deg(float angle_deg)
{
    while (angle_deg >= 180.0f) {
        angle_deg -= 360.0f;
    }
    while (angle_deg < -180.0f) {
        angle_deg += 360.0f;
    }
    return angle_deg;
}

static float vector_norm(const float vector[3])
{
    return sqrtf((vector[0] * vector[0]) +
                 (vector[1] * vector[1]) +
                 (vector[2] * vector[2]));
}

static float clamp_unit(float value)
{
    if (value < -1.0f) {
        return -1.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static void quaternion_normalize(float quaternion[4])
{
    float norm = sqrtf((quaternion[0] * quaternion[0]) +
                       (quaternion[1] * quaternion[1]) +
                       (quaternion[2] * quaternion[2]) +
                       (quaternion[3] * quaternion[3]));

    if (!isfinite(norm) || (norm < IMU_FUSION_QUATERNION_MIN_NORM)) {
        quaternion[0] = 1.0f;
        quaternion[1] = 0.0f;
        quaternion[2] = 0.0f;
        quaternion[3] = 0.0f;
        return;
    }
    quaternion[0] /= norm;
    quaternion[1] /= norm;
    quaternion[2] /= norm;
    quaternion[3] /= norm;
}

static bool quaternion_is_valid(const float quaternion[4])
{
    float norm_squared;

    if (!isfinite(quaternion[0]) || !isfinite(quaternion[1]) ||
        !isfinite(quaternion[2]) || !isfinite(quaternion[3])) {
        return false;
    }
    norm_squared = (quaternion[0] * quaternion[0]) +
                   (quaternion[1] * quaternion[1]) +
                   (quaternion[2] * quaternion[2]) +
                   (quaternion[3] * quaternion[3]);
    return isfinite(norm_squared) &&
           (norm_squared >= (IMU_FUSION_QUATERNION_MIN_NORM *
                             IMU_FUSION_QUATERNION_MIN_NORM));
}

static float gyro_raw_to_dps(int16_t raw_value)
{
    return (float)raw_value / IMU_FUSION_GYRO_LSB_PER_DPS;
}

static void convert_sample(const board_bmi160_sample_t *sample,
                           float acceleration_g[3], float gyro_dps[3])
{
    acceleration_g[0] = (float)sample->accel_x / IMU_FUSION_ACCEL_LSB_PER_G;
    acceleration_g[1] = (float)sample->accel_y / IMU_FUSION_ACCEL_LSB_PER_G;
    acceleration_g[2] = (float)sample->accel_z / IMU_FUSION_ACCEL_LSB_PER_G;
    gyro_dps[0] = gyro_raw_to_dps(sample->gyro_x);
    gyro_dps[1] = gyro_raw_to_dps(sample->gyro_y);
    gyro_dps[2] = IMU_FUSION_GYRO_Z_SIGN * gyro_raw_to_dps(sample->gyro_z);
}

static bool acceleration_is_valid(const float acceleration_g[3])
{
    float norm = vector_norm(acceleration_g);

    return isfinite(norm) &&
           (norm >= IMU_FUSION_ACCEL_NORM_MIN_G) &&
           (norm <= IMU_FUSION_ACCEL_NORM_MAX_G);
}

static bool gyro_is_below(const float gyro_dps[3], float limit_dps)
{
    return vector_norm(gyro_dps) <= limit_dps;
}

static void initialize_attitude_from_acceleration(
    imu_fusion_state_t *state, const float acceleration_g[3])
{
    float roll_rad = atan2f(acceleration_g[1], acceleration_g[2]);
    float horizontal_norm = sqrtf((acceleration_g[1] * acceleration_g[1]) +
                                  (acceleration_g[2] * acceleration_g[2]));
    float pitch_rad = atan2f(-acceleration_g[0], horizontal_norm);
    float half_roll = roll_rad * 0.5f;
    float half_pitch = pitch_rad * 0.5f;
    float cr = cosf(half_roll);
    float sr = sinf(half_roll);
    float cp = cosf(half_pitch);
    float sp = sinf(half_pitch);

    state->quaternion[0] = cr * cp;
    state->quaternion[1] = sr * cp;
    state->quaternion[2] = cr * sp;
    state->quaternion[3] = -sr * sp;
    quaternion_normalize(state->quaternion);
    state->yaw_deg = 0.0f;
    state->previous_yaw_deg = 0.0f;
    state->yaw_rate_dps = 0.0f;
    state->roll_deg = roll_rad * IMU_FUSION_RAD_TO_DEG;
    state->pitch_deg = pitch_rad * IMU_FUSION_RAD_TO_DEG;
}

static void reset_state(imu_fusion_state_t *state)
{
    uint32_t index;

    state->quaternion[0] = 1.0f;
    state->quaternion[1] = 0.0f;
    state->quaternion[2] = 0.0f;
    state->quaternion[3] = 0.0f;
    state->yaw_deg = 0.0f;
    state->yaw_rate_dps = 0.0f;
    state->roll_deg = 0.0f;
    state->pitch_deg = 0.0f;
    state->accel_norm_g = 0.0f;
    state->gyro_bias_x_dps = 0.0f;
    state->gyro_bias_y_dps = 0.0f;
    state->gyro_bias_z_dps = 0.0f;
    state->calibration_elapsed_s = 0.0f;
    state->stationary_elapsed_s = 0.0f;
    state->previous_yaw_deg = 0.0f;
    state->dt_s = 0.0f;
    state->calibration_samples = 0U;
    state->calibrated = false;
    state->stationary_confirmed = false;
    state->acceleration_valid = false;
    for (index = 0U; index < 3U; ++index) {
        state->calibration_m2_dps2[index] = 0.0f;
        state->gyro_integral[index] = 0.0f;
    }
}

static bool calibration_sample_is_valid(const imu_fusion_state_t *state,
                                        const float gyro_dps[3])
{
    uint32_t index;

    if (state->calibration_samples < IMU_FUSION_CALIBRATION_INITIAL_SAMPLES) {
        return true;
    }
    for (index = 0U; index < 3U; ++index) {
        float variance = state->calibration_m2_dps2[index] /
                         (float)(state->calibration_samples - 1U);
        float limit = IMU_FUSION_CALIBRATION_OUTLIER_MARGIN_DPS;
        float sigma_limit = 4.0f * sqrtf(fmaxf(variance, 0.0f));

        if (sigma_limit > limit) {
            limit = sigma_limit;
        }
        if (fabsf(gyro_dps[index] -
                  ((index == 0U) ? state->gyro_bias_x_dps :
                   (index == 1U) ? state->gyro_bias_y_dps :
                                   state->gyro_bias_z_dps)) > limit) {
            return false;
        }
    }
    return true;
}

static void accept_calibration_sample(imu_fusion_state_t *state,
                                      const float gyro_dps[3])
{
    float sample_count = (float)(state->calibration_samples + 1U);
    float means[3] = {state->gyro_bias_x_dps, state->gyro_bias_y_dps,
                      state->gyro_bias_z_dps};
    uint32_t index;

    for (index = 0U; index < 3U; ++index) {
        float delta = gyro_dps[index] - means[index];
        float updated_mean = means[index] + delta / sample_count;

        state->calibration_m2_dps2[index] +=
            delta * (gyro_dps[index] - updated_mean);
        means[index] = updated_mean;
    }
    state->gyro_bias_x_dps = means[0];
    state->gyro_bias_y_dps = means[1];
    state->gyro_bias_z_dps = means[2];
    ++state->calibration_samples;
}

static void integrate_quaternion(imu_fusion_state_t *state,
                                 const float gyro_rad_s[3], float dt_s)
{
    float q0 = state->quaternion[0];
    float q1 = state->quaternion[1];
    float q2 = state->quaternion[2];
    float q3 = state->quaternion[3];
    float half_dt = 0.5f * dt_s;

    state->quaternion[0] = q0 + (-q1 * gyro_rad_s[0] -
                                 q2 * gyro_rad_s[1] -
                                 q3 * gyro_rad_s[2]) * half_dt;
    state->quaternion[1] = q1 + (q0 * gyro_rad_s[0] +
                                 q2 * gyro_rad_s[2] -
                                 q3 * gyro_rad_s[1]) * half_dt;
    state->quaternion[2] = q2 + (q0 * gyro_rad_s[1] -
                                 q1 * gyro_rad_s[2] +
                                 q3 * gyro_rad_s[0]) * half_dt;
    state->quaternion[3] = q3 + (q0 * gyro_rad_s[2] +
                                 q1 * gyro_rad_s[1] -
                                 q2 * gyro_rad_s[0]) * half_dt;
    quaternion_normalize(state->quaternion);
}

static void apply_acceleration_correction(imu_fusion_state_t *state,
                                          const float acceleration_g[3],
                                          float gyro_rad_s[3], float dt_s)
{
    float q0 = state->quaternion[0];
    float q1 = state->quaternion[1];
    float q2 = state->quaternion[2];
    float q3 = state->quaternion[3];
    float accel_norm = vector_norm(acceleration_g);
    float ax = acceleration_g[0] / accel_norm;
    float ay = acceleration_g[1] / accel_norm;
    float az = acceleration_g[2] / accel_norm;
    float estimated_x = 2.0f * (q1 * q3 - q0 * q2);
    float estimated_y = 2.0f * (q0 * q1 + q2 * q3);
    float estimated_z = (q0 * q0) - (q1 * q1) - (q2 * q2) + (q3 * q3);
    float error_x = (ay * estimated_z) - (az * estimated_y);
    float error_y = (az * estimated_x) - (ax * estimated_z);
    float error_z = (ax * estimated_y) - (ay * estimated_x);

    state->gyro_integral[0] += IMU_FUSION_MAHONY_KI * error_x * dt_s;
    state->gyro_integral[1] += IMU_FUSION_MAHONY_KI * error_y * dt_s;
    state->gyro_integral[2] = 0.0f;
    gyro_rad_s[0] += (IMU_FUSION_MAHONY_KP * error_x) +
                     state->gyro_integral[0];
    gyro_rad_s[1] += (IMU_FUSION_MAHONY_KP * error_y) +
                     state->gyro_integral[1];
    gyro_rad_s[2] += IMU_FUSION_MAHONY_KP * error_z;
}

static float extract_yaw_deg(const float quaternion[4])
{
    float numerator = 2.0f * ((quaternion[0] * quaternion[3]) +
                              (quaternion[1] * quaternion[2]));
    float denominator = 1.0f - 2.0f * ((quaternion[2] * quaternion[2]) +
                                       (quaternion[3] * quaternion[3]));

    return normalize_angle_deg(atan2f(numerator, denominator) *
                               IMU_FUSION_RAD_TO_DEG);
}

static void extract_roll_pitch_deg(const float quaternion[4],
                                   float *roll_deg, float *pitch_deg)
{
    float roll_numerator = 2.0f * ((quaternion[0] * quaternion[1]) +
                                   (quaternion[2] * quaternion[3]));
    float roll_denominator = 1.0f - 2.0f * ((quaternion[1] * quaternion[1]) +
                                           (quaternion[2] * quaternion[2]));
    float pitch_argument = 2.0f * ((quaternion[0] * quaternion[2]) -
                                   (quaternion[3] * quaternion[1]));

    if ((roll_deg == NULL) || (pitch_deg == NULL)) {
        return;
    }
    *roll_deg = atan2f(roll_numerator, roll_denominator) *
                IMU_FUSION_RAD_TO_DEG;
    *pitch_deg = asinf(clamp_unit(pitch_argument)) * IMU_FUSION_RAD_TO_DEG;
}

void imu_fusion_init(imu_fusion_state_t *state)
{
    if (state == NULL) {
        return;
    }
    reset_state(state);
    g_recalibration_requested = false;
}

void imu_fusion_request_recalibration(void)
{
    g_recalibration_requested = true;
}

void imu_fusion_update(imu_fusion_state_t *state,
                       const board_bmi160_sample_t *imu_sample,
                       float dt_s)
{
    float acceleration_g[3];
    float gyro_dps[3];
    float corrected_gyro_dps[3];
    float gyro_rad_s[3];
    bool stationary;
    uint32_t index;

    if ((state == NULL) || (imu_sample == NULL) ||
        !isfinite(dt_s) || (dt_s <= 0.0f)) {
        return;
    }

    state->dt_s = dt_s;

    if (g_recalibration_requested) {
        reset_state(state);
        g_recalibration_requested = false;
    }
    if (!quaternion_is_valid(state->quaternion)) {
        reset_state(state);
        return;
    }

    convert_sample(imu_sample, acceleration_g, gyro_dps);
    state->accel_norm_g = vector_norm(acceleration_g);
    state->acceleration_valid = acceleration_is_valid(acceleration_g);
    if (!state->calibrated) {
        if (state->acceleration_valid &&
            gyro_is_below(gyro_dps, IMU_FUSION_CALIBRATION_GYRO_RATE_MAX_DPS) &&
            calibration_sample_is_valid(state, gyro_dps)) {
            accept_calibration_sample(state, gyro_dps);
            state->calibration_elapsed_s += dt_s;
            if (state->calibration_elapsed_s >=
                IMU_FUSION_STARTUP_CALIBRATION_TIME_S) {
                initialize_attitude_from_acceleration(state, acceleration_g);
                state->calibrated = true;
            }
        }
        state->stationary_confirmed = false;
        return;
    }

    for (index = 0U; index < 3U; ++index) {
        float bias = (index == 0U) ? state->gyro_bias_x_dps :
                     (index == 1U) ? state->gyro_bias_y_dps :
                                     state->gyro_bias_z_dps;

        corrected_gyro_dps[index] = gyro_dps[index] - bias;
        if (fabsf(corrected_gyro_dps[index]) <=
            IMU_FUSION_GYRO_RATE_DEADBAND_DPS) {
            corrected_gyro_dps[index] = 0.0f;
        }
        gyro_rad_s[index] = corrected_gyro_dps[index] * IMU_FUSION_DEG_TO_RAD;
    }

    stationary = state->acceleration_valid &&
                 gyro_is_below(corrected_gyro_dps,
                               IMU_FUSION_STATIONARY_GYRO_RATE_MAX_DPS);
    if (stationary) {
        state->stationary_elapsed_s += dt_s;
    } else {
        state->stationary_elapsed_s = 0.0f;
        state->stationary_confirmed = false;
    }

    if (state->stationary_elapsed_s >= IMU_FUSION_STATIONARY_CONFIRM_TIME_S) {
        float alpha = clamp01(dt_s / IMU_FUSION_BIAS_TIME_CONSTANT_S);

        state->gyro_bias_x_dps += alpha * (gyro_dps[0] - state->gyro_bias_x_dps);
        state->gyro_bias_y_dps += alpha * (gyro_dps[1] - state->gyro_bias_y_dps);
        state->gyro_bias_z_dps += alpha * (gyro_dps[2] - state->gyro_bias_z_dps);
        state->gyro_integral[0] = 0.0f;
        state->gyro_integral[1] = 0.0f;
        state->gyro_integral[2] = 0.0f;
        state->stationary_confirmed = true;
        state->yaw_rate_dps = 0.0f;
        state->previous_yaw_deg = state->yaw_deg;
    }

    if (state->acceleration_valid) {
        apply_acceleration_correction(state, acceleration_g, gyro_rad_s, dt_s);
    } else {
        state->gyro_integral[0] = 0.0f;
        state->gyro_integral[1] = 0.0f;
        state->gyro_integral[2] = 0.0f;
    }
    integrate_quaternion(state, gyro_rad_s, dt_s);
    if (!quaternion_is_valid(state->quaternion)) {
        reset_state(state);
        return;
    }
    state->yaw_deg = extract_yaw_deg(state->quaternion);
    extract_roll_pitch_deg(state->quaternion, &state->roll_deg,
                           &state->pitch_deg);
    state->yaw_rate_dps = normalize_angle_deg(state->yaw_deg -
                                               state->previous_yaw_deg) / dt_s;
    state->previous_yaw_deg = state->yaw_deg;
}
