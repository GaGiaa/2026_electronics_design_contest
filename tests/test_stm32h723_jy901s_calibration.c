#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "app_jy901s_calibration.h"

static app_jy901s_calibration_config_t make_config(void)
{
    app_jy901s_calibration_config_t config = {0};

    config.enable_gyro_calibration = true;
    config.vehicle_axis_sensor[0] = 0U;
    config.vehicle_axis_sensor[1] = 1U;
    config.vehicle_axis_sensor[2] = 2U;
    config.vehicle_axis_sign[0] = 1;
    config.vehicle_axis_sign[1] = 1;
    config.vehicle_axis_sign[2] = 1;
    config.angle_offset_deg[0] = 0.0f;
    config.angle_offset_deg[1] = 0.0f;
    config.angle_offset_deg[2] = 0.0f;
    config.sample_target = 2U;
    config.stationary_accel_min_g = 0.85f;
    config.stationary_accel_max_g = 1.15f;
    config.stationary_gyro_max_dps = 3.0f;
    config.timeout_ms = 100U;
    return config;
}

static app_jy901s_sample_t make_sample(float ax, float ay, float az,
                                       float gx, float gy, float gz,
                                       float roll, float pitch, float yaw)
{
    app_jy901s_sample_t sample = {0};

    sample.valid = true;
    sample.acceleration_g[0] = ax;
    sample.acceleration_g[1] = ay;
    sample.acceleration_g[2] = az;
    sample.angular_rate_dps[0] = gx;
    sample.angular_rate_dps[1] = gy;
    sample.angular_rate_dps[2] = gz;
    sample.angle_deg[0] = roll;
    sample.angle_deg[1] = pitch;
    sample.angle_deg[2] = yaw;
    return sample;
}

static void test_identity_mapping_preserves_vectors_and_wraps_angles(void)
{
    app_jy901s_calibration_config_t config = make_config();
    app_jy901s_calibration_t calibration;
    app_jy901s_calibration_output_t output;
    app_jy901s_sample_t sample = make_sample(1.0f, 2.0f, 3.0f,
                                             4.0f, 5.0f, 6.0f,
                                             179.0f, -179.0f, 10.0f);

    config.enable_gyro_calibration = false;
    config.angle_offset_deg[0] = 5.0f;
    config.angle_offset_deg[1] = -5.0f;
    assert(app_jy901s_calibration_init(&calibration, &config, 0U));
    assert(app_jy901s_calibration_update(&calibration, &sample, 0U, &output));
    assert(fabsf(output.acceleration_g[0] - 1.0f) < 0.001f);
    assert(fabsf(output.angular_rate_dps[2] - 6.0f) < 0.001f);
    assert(fabsf(output.angle_deg[0] + 176.0f) < 0.001f);
    assert(fabsf(output.angle_deg[1] - 176.0f) < 0.001f);
    assert(output.calibration_valid);
}

static void test_axis_mapping_and_stationary_samples_estimate_vehicle_bias(void)
{
    app_jy901s_calibration_config_t config = make_config();
    app_jy901s_calibration_t calibration;
    app_jy901s_calibration_output_t output;
    app_jy901s_sample_t first = make_sample(0.0f, 0.0f, 1.0f,
                                             1.0f, 2.0f, 3.0f,
                                             1.0f, 2.0f, 3.0f);
    app_jy901s_sample_t second = make_sample(0.0f, 0.0f, 1.0f,
                                              3.0f, 4.0f, 5.0f,
                                              1.0f, 2.0f, 3.0f);

    config.vehicle_axis_sensor[0] = 2U;
    config.vehicle_axis_sensor[1] = 0U;
    config.vehicle_axis_sensor[2] = 1U;
    config.vehicle_axis_sign[0] = -1;
    config.vehicle_axis_sign[1] = 1;
    config.vehicle_axis_sign[2] = -1;
    config.stationary_gyro_max_dps = 6.0f;
    assert(app_jy901s_calibration_init(&calibration, &config, 0U));
    assert(app_jy901s_calibration_update(&calibration, &first, 0U, &output));
    assert(!output.calibration_valid);
    assert(app_jy901s_calibration_update(&calibration, &second, 5U, &output));
    assert(output.calibration_valid);
    assert(fabsf(output.gyro_bias_dps[0] + 4.0f) < 0.001f);
    assert(fabsf(output.gyro_bias_dps[1] - 2.0f) < 0.001f);
    assert(fabsf(output.gyro_bias_dps[2] + 3.0f) < 0.001f);
    assert(fabsf(output.angular_rate_dps[0] + 1.0f) < 0.001f);
    assert(fabsf(output.angular_rate_dps[1] - 1.0f) < 0.001f);
    assert(fabsf(output.angular_rate_dps[2] + 1.0f) < 0.001f);
}

static void test_motion_resets_continuous_count_and_timeout_fails(void)
{
    app_jy901s_calibration_config_t config = make_config();
    app_jy901s_calibration_t calibration;
    app_jy901s_calibration_output_t output;
    app_jy901s_sample_t moving = make_sample(0.0f, 0.0f, 2.0f,
                                              0.0f, 0.0f, 0.0f,
                                              0.0f, 0.0f, 0.0f);

    assert(app_jy901s_calibration_init(&calibration, &config, 0U));
    assert(app_jy901s_calibration_update(&calibration, &moving, 10U, &output));
    assert(output.calibration_reason == APP_JY901S_CALIBRATION_REASON_MOVING);
    assert(output.calibration_sample_count == 0U);
    assert(app_jy901s_calibration_update(&calibration, &moving, 101U, &output));
    assert(output.calibration_status == APP_JY901S_CALIBRATION_STATUS_FAILED);
    assert(output.calibration_reason == APP_JY901S_CALIBRATION_REASON_TIMEOUT);
    assert(!output.calibration_valid);
}

static void test_invalid_axis_mapping_is_rejected(void)
{
    app_jy901s_calibration_config_t config = make_config();
    app_jy901s_calibration_t calibration;

    config.vehicle_axis_sensor[2] = 1U;
    assert(!app_jy901s_calibration_init(&calibration, &config, 0U));
    assert(calibration.status == APP_JY901S_CALIBRATION_STATUS_FAILED);
    assert(calibration.reason == APP_JY901S_CALIBRATION_REASON_CONFIG);
}

int main(void)
{
    test_identity_mapping_preserves_vectors_and_wraps_angles();
    test_axis_mapping_and_stationary_samples_estimate_vehicle_bias();
    test_motion_resets_continuous_count_and_timeout_fails();
    test_invalid_axis_mapping_is_rejected();
    return 0;
}
