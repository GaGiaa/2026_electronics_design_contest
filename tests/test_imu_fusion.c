#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "algorithms/imu_fusion/imu_fusion.h"

#define TEST_DT_S 0.005f
#define TEST_GYRO_LSB_PER_DPS 65.6f
#define TEST_ACCEL_LSB_PER_G 8192.0f
#define TEST_PI 3.14159265358979323846f

static int16_t to_raw(float value, float scale)
{
    return (int16_t)lroundf(value * scale);
}

static board_bmi160_sample_t sample_from(float accel_x_g, float accel_y_g,
                                          float accel_z_g, float gyro_x_dps,
                                          float gyro_y_dps, float gyro_z_dps)
{
    board_bmi160_sample_t sample = {0};

    sample.accel_x = to_raw(accel_x_g, TEST_ACCEL_LSB_PER_G);
    sample.accel_y = to_raw(accel_y_g, TEST_ACCEL_LSB_PER_G);
    sample.accel_z = to_raw(accel_z_g, TEST_ACCEL_LSB_PER_G);
    sample.gyro_x = to_raw(gyro_x_dps, TEST_GYRO_LSB_PER_DPS);
    sample.gyro_y = to_raw(gyro_y_dps, TEST_GYRO_LSB_PER_DPS);
    sample.gyro_z = to_raw(gyro_z_dps, TEST_GYRO_LSB_PER_DPS);
    return sample;
}

static void update_many(imu_fusion_state_t *state,
                        const board_bmi160_sample_t *sample,
                        uint32_t count, float dt_s)
{
    uint32_t index;

    for (index = 0U; index < count; ++index) {
        imu_fusion_update(state, sample, dt_s);
    }
}

static void complete_calibration(imu_fusion_state_t *state)
{
    board_bmi160_sample_t sample = sample_from(0.0f, 0.0f, 1.0f,
                                                0.8f, -0.6f, 1.2f);

    update_many(state, &sample, 240U, TEST_DT_S);
    assert(state->calibrated);
    assert(fabsf(state->gyro_bias_x_dps - 0.8f) < 0.1f);
    assert(fabsf(state->gyro_bias_y_dps + 0.6f) < 0.1f);
    assert(fabsf(state->gyro_bias_z_dps - 1.2f) < 0.1f);
}

static void test_calibrates_three_axis_bias(void)
{
    imu_fusion_state_t state;

    imu_fusion_init(&state);
    complete_calibration(&state);
}

static void test_recalibration_invalidates_and_restarts(void)
{
    imu_fusion_state_t state;
    board_bmi160_sample_t sample = sample_from(0.0f, 0.0f, 1.0f,
                                                0.0f, 0.0f, 0.0f);

    imu_fusion_init(&state);
    complete_calibration(&state);
    imu_fusion_request_recalibration();
    imu_fusion_update(&state, &sample, TEST_DT_S);
    assert(!state.calibrated);
    assert(state.calibration_samples == 1U);
}

static void test_calibration_rejects_outlier(void)
{
    imu_fusion_state_t state;
    board_bmi160_sample_t stable = sample_from(0.0f, 0.0f, 1.0f,
                                                0.0f, 0.0f, 0.0f);
    board_bmi160_sample_t outlier = sample_from(0.0f, 0.0f, 1.0f,
                                                 0.0f, 0.0f, 2.4f);

    imu_fusion_init(&state);
    update_many(&state, &stable, IMU_FUSION_CALIBRATION_INITIAL_SAMPLES,
                TEST_DT_S);
    imu_fusion_update(&state, &outlier, TEST_DT_S);
    assert(state.calibration_samples == IMU_FUSION_CALIBRATION_INITIAL_SAMPLES);
    update_many(&state, &stable, 190U, TEST_DT_S);
    assert(state.calibrated);
    assert(fabsf(state.gyro_bias_z_dps) < 0.1f);
}

static void test_gyro_integrates_yaw_without_encoder_input(void)
{
    imu_fusion_state_t state;
    board_bmi160_sample_t sample;

    imu_fusion_init(&state);
    complete_calibration(&state);
    sample = sample_from(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 90.0f + 1.2f);
    update_many(&state, &sample, 200U, TEST_DT_S);
    assert(fabsf(state.yaw_deg - 90.0f) < 1.0f);
}

static void test_yaw_wraps_to_signed_range(void)
{
    imu_fusion_state_t state;
    board_bmi160_sample_t sample;

    imu_fusion_init(&state);
    complete_calibration(&state);
    sample = sample_from(0.0f, 0.0f, 1.0f, 0.8f, -0.6f, 181.2f);
    update_many(&state, &sample, 200U, TEST_DT_S);
    assert((state.yaw_deg >= -180.0f) && (state.yaw_deg < 180.0f));
    assert(fabsf(fabsf(state.yaw_deg) - 179.0f) < 1.5f);
}

static void test_static_tilt_does_not_create_yaw(void)
{
    imu_fusion_state_t state;
    board_bmi160_sample_t sample;
    float tilt_rad = 30.0f * TEST_PI / 180.0f;

    imu_fusion_init(&state);
    complete_calibration(&state);
    sample = sample_from(0.0f, -sinf(tilt_rad), cosf(tilt_rad),
                         0.8f, -0.6f, 1.2f);
    update_many(&state, &sample, 400U, TEST_DT_S);
    assert(fabsf(state.yaw_deg) < 1.0f);
    assert(fabsf(state.roll_deg + 30.0f) < 2.0f);
    assert(fabsf(state.pitch_deg) < 2.0f);
    assert(fabsf(state.accel_norm_g - 1.0f) < 0.02f);
    assert(fabsf(state.dt_s - TEST_DT_S) < 0.00001f);
}

static void test_tilted_world_vertical_rotation_tracks_yaw(void)
{
    imu_fusion_state_t state;
    board_bmi160_sample_t tilt_sample;
    board_bmi160_sample_t rotation_sample;
    float tilt_rad = 30.0f * TEST_PI / 180.0f;
    float yaw_rate_dps = 45.0f;

    imu_fusion_init(&state);
    complete_calibration(&state);
    tilt_sample = sample_from(0.0f, -sinf(tilt_rad), cosf(tilt_rad),
                              0.8f, -0.6f, 1.2f);
    update_many(&state, &tilt_sample, 400U, TEST_DT_S);
    rotation_sample = sample_from(
        0.0f, -sinf(tilt_rad), cosf(tilt_rad),
        0.8f, -0.6f - yaw_rate_dps * sinf(tilt_rad),
        1.2f + yaw_rate_dps * cosf(tilt_rad));
    update_many(&state, &rotation_sample, 200U, TEST_DT_S);
    assert(fabsf(state.yaw_deg - 45.0f) < 3.0f);
}

static void test_invalid_acceleration_falls_back_to_gyro(void)
{
    imu_fusion_state_t state;
    board_bmi160_sample_t sample;

    imu_fusion_init(&state);
    complete_calibration(&state);
    sample = sample_from(0.0f, 0.0f, 2.0f, 0.8f, -0.6f, 46.2f);
    update_many(&state, &sample, 200U, TEST_DT_S);
    assert(!state.acceleration_valid);
    assert(fabsf(state.yaw_deg - 45.0f) < 1.5f);
}

static void test_direction_error_rejects_linear_acceleration(void)
{
    imu_fusion_state_t state;
    board_bmi160_sample_t accelerated = sample_from(0.75f, 0.0f, 1.0f,
                                                     0.8f, -0.6f, 1.2f);
    board_bmi160_sample_t gravity = sample_from(0.0f, 0.0f, 1.0f,
                                                 0.8f, -0.6f, 1.2f);

    imu_fusion_init(&state);
    complete_calibration(&state);
    update_many(&state, &accelerated, 200U, TEST_DT_S);
    assert(!state.acceleration_valid);
    assert(fabsf(state.pitch_deg) < 3.0f);

    update_many(&state, &gravity, 20U, TEST_DT_S);
    assert(state.acceleration_valid);
    assert(fabsf(state.pitch_deg) < 3.0f);
}

static void test_sustained_direction_error_enters_recovery(void)
{
    imu_fusion_state_t state;
    board_bmi160_sample_t accelerated = sample_from(0.75f, 0.0f, 1.0f,
                                                     0.8f, -0.6f, 1.2f);

    imu_fusion_init(&state);
    complete_calibration(&state);
    update_many(&state, &accelerated, 600U, TEST_DT_S);
    assert(state.acceleration_recovery_active);
    assert(state.acceleration_valid);
}

static void test_actual_dt_controls_yaw_rate(void)
{
    imu_fusion_state_t state;
    board_bmi160_sample_t sample;

    imu_fusion_init(&state);
    complete_calibration(&state);
    sample = sample_from(0.0f, 0.0f, 1.0f, 0.8f, -0.6f, 91.2f);
    update_many(&state, &sample, 10U, 0.01f);
    assert(fabsf(state.yaw_deg - 9.0f) < 0.5f);
    assert(fabsf(state.yaw_rate_dps - 90.0f) < 2.0f);
}

static void test_invalid_dt_and_nonfinite_sample_are_ignored(void)
{
    imu_fusion_state_t state;
    board_bmi160_sample_t sample = sample_from(0.0f, 0.0f, 1.0f,
                                                0.0f, 0.0f, 0.0f);
    float yaw_before;

    imu_fusion_init(&state);
    complete_calibration(&state);
    yaw_before = state.yaw_deg;
    imu_fusion_update(&state, &sample, 0.0f);
    assert(state.yaw_deg == yaw_before);
    sample.accel_x = INT16_MAX;
    sample.accel_y = INT16_MIN;
    imu_fusion_update(&state, &sample, NAN);
    assert(state.yaw_deg == yaw_before);
}

static void test_invalid_quaternion_restarts_calibration(void)
{
    imu_fusion_state_t state;
    board_bmi160_sample_t sample = sample_from(0.0f, 0.0f, 1.0f,
                                                0.0f, 0.0f, 0.0f);

    imu_fusion_init(&state);
    complete_calibration(&state);
    state.quaternion[0] = NAN;
    imu_fusion_update(&state, &sample, TEST_DT_S);
    assert(!state.calibrated);
    assert(state.calibration_samples == 0U);

    complete_calibration(&state);
    state.quaternion[0] = 0.0f;
    state.quaternion[1] = 0.0f;
    state.quaternion[2] = 0.0f;
    state.quaternion[3] = 0.0f;
    imu_fusion_update(&state, &sample, TEST_DT_S);
    assert(!state.calibrated);
    assert(state.calibration_samples == 0U);
}

int main(void)
{
    test_calibrates_three_axis_bias();
    test_recalibration_invalidates_and_restarts();
    test_calibration_rejects_outlier();
    test_gyro_integrates_yaw_without_encoder_input();
    test_yaw_wraps_to_signed_range();
    test_static_tilt_does_not_create_yaw();
    test_tilted_world_vertical_rotation_tracks_yaw();
    test_invalid_acceleration_falls_back_to_gyro();
    test_direction_error_rejects_linear_acceleration();
    test_sustained_direction_error_enters_recovery();
    test_actual_dt_controls_yaw_rate();
    test_invalid_dt_and_nonfinite_sample_are_ignored();
    test_invalid_quaternion_restarts_calibration();
    puts("imu_fusion tests passed");
    return 0;
}
