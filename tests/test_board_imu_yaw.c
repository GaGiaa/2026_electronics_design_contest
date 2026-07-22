#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "board_imu_yaw.h"

#define TEST_DT_S 0.01f
#define TEST_TRACK_WIDTH_MM 100.0f
#define TEST_GYRO_LSB_PER_DPS 65.6f
#define TEST_ACCEL_LSB_PER_G 8192.0f

static board_bmi160_sample_t stationary_sample(float gyro_z_dps)
{
    board_bmi160_sample_t sample = {0};

    sample.accel_z = (int16_t)TEST_ACCEL_LSB_PER_G;
    sample.gyro_z = (int16_t)(gyro_z_dps * TEST_GYRO_LSB_PER_DPS);
    return sample;
}

static board_encoder_sample_t encoder_sample(float speed_mm_per_s)
{
    board_encoder_sample_t sample = {0};

    sample.speed_mm_per_s = speed_mm_per_s;
    return sample;
}

static void complete_startup_calibration(board_imu_yaw_state_t *state,
                                         float gyro_bias_dps)
{
    board_bmi160_sample_t sample = stationary_sample(gyro_bias_dps);
    board_encoder_sample_t encoders[BOARD_MOTOR_COUNT] = {0};
    uint32_t index;

    for (index = 0U; index < BOARD_IMU_YAW_STARTUP_CALIBRATION_SAMPLES; ++index) {
        board_imu_yaw_update(state, &sample, encoders, TEST_DT_S);
    }
    assert(state->calibrated);
    assert(fabsf(state->gyro_bias_z_dps - gyro_bias_dps) < 0.1f);
}

static void test_stationary_bias_does_not_drift(void)
{
    board_imu_yaw_state_t state;
    board_bmi160_sample_t sample = stationary_sample(1.0f);
    board_encoder_sample_t encoders[BOARD_MOTOR_COUNT] = {0};
    uint32_t index;

    board_imu_yaw_init(&state, TEST_TRACK_WIDTH_MM);
    complete_startup_calibration(&state, 1.0f);
    for (index = 0U; index < 300U; ++index) {
        board_imu_yaw_update(&state, &sample, encoders, TEST_DT_S);
    }
    assert(fabsf(state.yaw_deg) < 0.1f);
}

static void test_stationary_after_rotation_holds_yaw(void)
{
    board_imu_yaw_state_t state;
    board_bmi160_sample_t rotating_sample = stationary_sample(90.0f);
    board_bmi160_sample_t stopped_sample = stationary_sample(5.0f);
    board_encoder_sample_t rotating_encoders[BOARD_MOTOR_COUNT];
    board_encoder_sample_t stopped_encoders[BOARD_MOTOR_COUNT] = {0};
    float yaw_after_stationary_confirmation;
    uint32_t index;

    board_imu_yaw_init(&state, TEST_TRACK_WIDTH_MM);
    complete_startup_calibration(&state, 0.0f);
    for (index = 0U; index < BOARD_MOTOR_COUNT; ++index) {
        rotating_encoders[index] = encoder_sample(100.0f);
    }
    for (index = 0U; index < 20U; ++index) {
        board_imu_yaw_update(&state, &rotating_sample, rotating_encoders,
                             TEST_DT_S);
    }
    for (index = 0U; index < BOARD_IMU_YAW_STATIONARY_CONFIRM_SAMPLES;
         ++index) {
        board_imu_yaw_update(&state, &stopped_sample, stopped_encoders,
                             TEST_DT_S);
    }
    yaw_after_stationary_confirmation = state.yaw_deg;

    for (index = 0U; index < 500U; ++index) {
        board_imu_yaw_update(&state, &stopped_sample, stopped_encoders,
                             TEST_DT_S);
    }

    assert(fabsf(state.yaw_deg - yaw_after_stationary_confirmation) < 0.1f);
    assert(fabsf(state.yaw_rate_dps) < 0.01f);
}

static void test_gyro_integrates_yaw(void)
{
    board_imu_yaw_state_t state;
    board_bmi160_sample_t sample = stationary_sample(90.0f);
    board_encoder_sample_t encoders[BOARD_MOTOR_COUNT];
    uint32_t index;

    board_imu_yaw_init(&state, TEST_TRACK_WIDTH_MM);
    complete_startup_calibration(&state, 0.0f);
    for (index = 0U; index < BOARD_MOTOR_COUNT; ++index) {
        encoders[index] = encoder_sample(100.0f);
    }
    for (index = 0U; index < 100U; ++index) {
        board_imu_yaw_update(&state, &sample, encoders, TEST_DT_S);
    }
    assert(fabsf(state.yaw_deg - 88.2f) < 0.5f);
}

static void test_encoder_turn_direction(void)
{
    board_imu_yaw_state_t state;
    board_bmi160_sample_t sample = stationary_sample(0.0f);
    board_encoder_sample_t encoders[BOARD_MOTOR_COUNT];
    float expected_rate_dps;

    board_imu_yaw_init(&state, TEST_TRACK_WIDTH_MM);
    complete_startup_calibration(&state, 0.0f);
    encoders[BOARD_MOTOR_FRONT_LEFT] = encoder_sample(0.0f);
    encoders[BOARD_MOTOR_REAR_LEFT] = encoder_sample(0.0f);
    encoders[BOARD_MOTOR_FRONT_RIGHT] = encoder_sample(1000.0f);
    encoders[BOARD_MOTOR_REAR_RIGHT] = encoder_sample(1000.0f);
    board_imu_yaw_update(&state, &sample, encoders, TEST_DT_S);
    expected_rate_dps = 0.02f * (1000.0f / TEST_TRACK_WIDTH_MM) * 57.2957795f;
    assert(state.yaw_rate_dps > 0.0f);
    assert(fabsf(state.yaw_rate_dps - expected_rate_dps) < 0.2f);
}

static void test_yaw_wraps_to_signed_range(void)
{
    board_imu_yaw_state_t state;
    board_bmi160_sample_t sample = stationary_sample(180.0f);
    board_encoder_sample_t encoders[BOARD_MOTOR_COUNT];
    uint32_t index;

    board_imu_yaw_init(&state, TEST_TRACK_WIDTH_MM);
    complete_startup_calibration(&state, 0.0f);
    for (index = 0U; index < BOARD_MOTOR_COUNT; ++index) {
        encoders[index] = encoder_sample(100.0f);
    }
    for (index = 0U; index < 200U; ++index) {
        board_imu_yaw_update(&state, &sample, encoders, TEST_DT_S);
    }
    assert((state.yaw_deg >= -180.0f) && (state.yaw_deg < 180.0f));
    assert(fabsf(state.yaw_deg + 7.2f) < 0.8f);
}

int main(void)
{
    test_stationary_bias_does_not_drift();
    test_stationary_after_rotation_holds_yaw();
    test_gyro_integrates_yaw();
    test_encoder_turn_direction();
    test_yaw_wraps_to_signed_range();
    puts("board_imu_yaw tests passed");
    return 0;
}
