#ifndef APP_JY901S_CALIBRATION_H
#define APP_JY901S_CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>

#include "app_jy901s.h"

#define APP_JY901S_CALIBRATION_AXIS_COUNT 3U

typedef enum {
    APP_JY901S_CALIBRATION_STATUS_COLLECTING = 0U,
    APP_JY901S_CALIBRATION_STATUS_VALID = 1U,
    APP_JY901S_CALIBRATION_STATUS_FAILED = 2U
} app_jy901s_calibration_status_t;

typedef enum {
    APP_JY901S_CALIBRATION_REASON_NONE = 0U,
    APP_JY901S_CALIBRATION_REASON_MOVING = 1U,
    APP_JY901S_CALIBRATION_REASON_TIMEOUT = 2U,
    APP_JY901S_CALIBRATION_REASON_CONFIG = 3U
} app_jy901s_calibration_reason_t;

typedef struct {
    bool enable_gyro_calibration;
    uint8_t vehicle_axis_sensor[APP_JY901S_CALIBRATION_AXIS_COUNT];
    int8_t vehicle_axis_sign[APP_JY901S_CALIBRATION_AXIS_COUNT];
    float angle_offset_deg[APP_JY901S_CALIBRATION_AXIS_COUNT];
    uint32_t sample_target;
    float stationary_accel_min_g;
    float stationary_accel_max_g;
    float stationary_gyro_max_dps;
    uint32_t timeout_ms;
} app_jy901s_calibration_config_t;

typedef struct {
    app_jy901s_calibration_status_t status;
    app_jy901s_calibration_reason_t reason;
    uint32_t calibration_sample_count;
    bool calibration_valid;
    float gyro_bias_dps[APP_JY901S_CALIBRATION_AXIS_COUNT];
    app_jy901s_calibration_config_t config;
    float gyro_sum_dps[APP_JY901S_CALIBRATION_AXIS_COUNT];
    uint32_t start_ms;
} app_jy901s_calibration_t;

typedef struct {
    float acceleration_g[APP_JY901S_CALIBRATION_AXIS_COUNT];
    float angular_rate_dps[APP_JY901S_CALIBRATION_AXIS_COUNT];
    float angle_deg[APP_JY901S_CALIBRATION_AXIS_COUNT];
    float gyro_bias_dps[APP_JY901S_CALIBRATION_AXIS_COUNT];
    app_jy901s_calibration_status_t calibration_status;
    app_jy901s_calibration_reason_t calibration_reason;
    uint32_t calibration_sample_count;
    bool calibration_valid;
} app_jy901s_calibration_output_t;

bool app_jy901s_calibration_init(app_jy901s_calibration_t *calibration,
                                 const app_jy901s_calibration_config_t *config,
                                 uint32_t now_ms);
bool app_jy901s_calibration_update(app_jy901s_calibration_t *calibration,
                                    const app_jy901s_sample_t *sample,
                                    uint32_t now_ms,
                                    app_jy901s_calibration_output_t *output);

#endif
