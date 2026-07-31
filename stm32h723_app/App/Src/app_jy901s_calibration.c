#include "app_jy901s_calibration.h"

#include <math.h>
#include <string.h>

static bool app_jy901s_calibration_config_valid(const app_jy901s_calibration_config_t *config)
{
    uint8_t used_axes = 0U;
    uint32_t index;

    if (config == NULL || config->sample_target == 0U || config->timeout_ms == 0U ||
        config->stationary_accel_min_g > config->stationary_accel_max_g ||
        config->stationary_gyro_max_dps <= 0.0f) {
        return false;
    }
    for (index = 0U; index < APP_JY901S_CALIBRATION_AXIS_COUNT; ++index) {
        uint8_t axis = config->vehicle_axis_sensor[index];
        uint8_t axis_bit;

        if (axis >= APP_JY901S_CALIBRATION_AXIS_COUNT ||
            (config->vehicle_axis_sign[index] != 1 && config->vehicle_axis_sign[index] != -1)) {
            return false;
        }
        axis_bit = (uint8_t)(1U << axis);
        if ((used_axes & axis_bit) != 0U) { return false; }
        used_axes = (uint8_t)(used_axes | axis_bit);
    }
    return true;
}

static float app_jy901s_calibration_wrap_angle(float angle_deg)
{
    while (angle_deg >= 180.0f) { angle_deg -= 360.0f; }
    while (angle_deg < -180.0f) { angle_deg += 360.0f; }
    return angle_deg;
}

static void app_jy901s_calibration_map_vector(const app_jy901s_calibration_t *calibration,
                                              const float sensor_values[APP_JY901S_CALIBRATION_AXIS_COUNT],
                                              float vehicle_values[APP_JY901S_CALIBRATION_AXIS_COUNT])
{
    uint32_t index;

    for (index = 0U; index < APP_JY901S_CALIBRATION_AXIS_COUNT; ++index) {
        vehicle_values[index] = (float)calibration->config.vehicle_axis_sign[index] *
            sensor_values[calibration->config.vehicle_axis_sensor[index]];
    }
}

static bool app_jy901s_calibration_sample_is_stationary(
    const app_jy901s_calibration_t *calibration,
    const float acceleration_g[APP_JY901S_CALIBRATION_AXIS_COUNT],
    const float angular_rate_dps[APP_JY901S_CALIBRATION_AXIS_COUNT])
{
    float acceleration_norm = sqrtf(acceleration_g[0] * acceleration_g[0] +
                                    acceleration_g[1] * acceleration_g[1] +
                                    acceleration_g[2] * acceleration_g[2]);
    uint32_t index;

    if (!isfinite(acceleration_norm) ||
        acceleration_norm < calibration->config.stationary_accel_min_g ||
        acceleration_norm > calibration->config.stationary_accel_max_g) {
        return false;
    }
    for (index = 0U; index < APP_JY901S_CALIBRATION_AXIS_COUNT; ++index) {
        if (!isfinite(angular_rate_dps[index]) ||
            fabsf(angular_rate_dps[index]) > calibration->config.stationary_gyro_max_dps) {
            return false;
        }
    }
    return true;
}

static void app_jy901s_calibration_fill_output(const app_jy901s_calibration_t *calibration,
                                               app_jy901s_calibration_output_t *output)
{
    uint32_t index;

    output->calibration_status = calibration->status;
    output->calibration_reason = calibration->reason;
    output->calibration_sample_count = calibration->calibration_sample_count;
    output->calibration_valid = calibration->calibration_valid;
    for (index = 0U; index < APP_JY901S_CALIBRATION_AXIS_COUNT; ++index) {
        output->gyro_bias_dps[index] = calibration->gyro_bias_dps[index];
    }
}

bool app_jy901s_calibration_init(app_jy901s_calibration_t *calibration,
                                 const app_jy901s_calibration_config_t *config,
                                 uint32_t now_ms)
{
    if (calibration == NULL) { return false; }
    memset(calibration, 0, sizeof(*calibration));
    if (!app_jy901s_calibration_config_valid(config)) {
        calibration->status = APP_JY901S_CALIBRATION_STATUS_FAILED;
        calibration->reason = APP_JY901S_CALIBRATION_REASON_CONFIG;
        return false;
    }
    calibration->config = *config;
    calibration->start_ms = now_ms;
    if (config->enable_gyro_calibration) {
        calibration->status = APP_JY901S_CALIBRATION_STATUS_COLLECTING;
    } else {
        calibration->status = APP_JY901S_CALIBRATION_STATUS_VALID;
        calibration->calibration_valid = true;
    }
    return true;
}

bool app_jy901s_calibration_update(app_jy901s_calibration_t *calibration,
                                   const app_jy901s_sample_t *sample,
                                   uint32_t now_ms,
                                   app_jy901s_calibration_output_t *output)
{
    float sensor_acceleration[APP_JY901S_CALIBRATION_AXIS_COUNT];
    float sensor_angular_rate[APP_JY901S_CALIBRATION_AXIS_COUNT];
    float sensor_angle[APP_JY901S_CALIBRATION_AXIS_COUNT];
    float vehicle_acceleration[APP_JY901S_CALIBRATION_AXIS_COUNT];
    float vehicle_angular_rate[APP_JY901S_CALIBRATION_AXIS_COUNT];
    float vehicle_angle[APP_JY901S_CALIBRATION_AXIS_COUNT];
    uint32_t index;

    if (calibration == NULL || sample == NULL || output == NULL || !sample->valid) {
        return false;
    }

    for (index = 0U; index < APP_JY901S_CALIBRATION_AXIS_COUNT; ++index) {
        sensor_acceleration[index] = sample->acceleration_g[index];
        sensor_angular_rate[index] = sample->angular_rate_dps[index];
        sensor_angle[index] = sample->angle_deg[index];
    }
    app_jy901s_calibration_map_vector(calibration, sensor_acceleration, vehicle_acceleration);
    app_jy901s_calibration_map_vector(calibration, sensor_angular_rate, vehicle_angular_rate);
    app_jy901s_calibration_map_vector(calibration, sensor_angle, vehicle_angle);

    if (calibration->status == APP_JY901S_CALIBRATION_STATUS_COLLECTING &&
        (now_ms - calibration->start_ms) > calibration->config.timeout_ms) {
        calibration->status = APP_JY901S_CALIBRATION_STATUS_FAILED;
        calibration->reason = APP_JY901S_CALIBRATION_REASON_TIMEOUT;
    }

    if (calibration->status == APP_JY901S_CALIBRATION_STATUS_COLLECTING) {
        if (!app_jy901s_calibration_sample_is_stationary(calibration,
                                                         vehicle_acceleration,
                                                         vehicle_angular_rate)) {
            calibration->calibration_sample_count = 0U;
            memset(calibration->gyro_sum_dps, 0, sizeof(calibration->gyro_sum_dps));
            calibration->reason = APP_JY901S_CALIBRATION_REASON_MOVING;
        } else {
            for (index = 0U; index < APP_JY901S_CALIBRATION_AXIS_COUNT; ++index) {
                calibration->gyro_sum_dps[index] += vehicle_angular_rate[index];
            }
            ++calibration->calibration_sample_count;
            calibration->reason = APP_JY901S_CALIBRATION_REASON_NONE;
            if (calibration->calibration_sample_count >= calibration->config.sample_target) {
                for (index = 0U; index < APP_JY901S_CALIBRATION_AXIS_COUNT; ++index) {
                    calibration->gyro_bias_dps[index] = calibration->gyro_sum_dps[index] /
                        (float)calibration->calibration_sample_count;
                }
                calibration->status = APP_JY901S_CALIBRATION_STATUS_VALID;
                calibration->calibration_valid = true;
            }
        }
    }

    for (index = 0U; index < APP_JY901S_CALIBRATION_AXIS_COUNT; ++index) {
        output->acceleration_g[index] = vehicle_acceleration[index];
        output->angular_rate_dps[index] = vehicle_angular_rate[index] -
            (calibration->calibration_valid ? calibration->gyro_bias_dps[index] : 0.0f);
        output->angle_deg[index] = app_jy901s_calibration_wrap_angle(
            vehicle_angle[index] + calibration->config.angle_offset_deg[index]);
    }
    app_jy901s_calibration_fill_output(calibration, output);
    return true;
}
