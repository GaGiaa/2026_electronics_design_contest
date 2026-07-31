#ifndef APP_WHEEL_ODOMETRY_H
#define APP_WHEEL_ODOMETRY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float wheel_diameter_mm;
    float track_width_mm;
    float left_encoder_sign;
    float right_encoder_sign;
} app_wheel_odometry_config_t;

typedef struct {
    uint32_t now_ms;
    bool left_feedback_valid;
    bool right_feedback_valid;
    uint32_t left_feedback_age_ms;
    uint32_t right_feedback_age_ms;
    int64_t left_motor_counts;
    int64_t right_motor_counts;
    bool reset_request;
} app_wheel_odometry_input_t;

typedef struct {
    float wheel_diameter_mm;
    float track_width_mm;
    float left_encoder_sign;
    float right_encoder_sign;
    bool params_valid;
    uint32_t params_rejected_count;
    bool initialized;
    bool valid;
    uint32_t reset_count;
    uint32_t rebaseline_count;
    uint32_t sample_count;
    int64_t left_motor_counts;
    int64_t right_motor_counts;
    float left_wheel_distance_mm;
    float right_wheel_distance_mm;
    float delta_left_mm;
    float delta_right_mm;
    float delta_distance_mm;
    float delta_yaw_deg;
    float x_mm;
    float y_mm;
    float yaw_deg;
    float yaw_deg_continuous;
    float linear_speed_mm_s;
    float angular_speed_deg_s;
    uint32_t left_feedback_age_ms;
    uint32_t right_feedback_age_ms;
} app_wheel_odometry_output_t;

typedef struct {
    app_wheel_odometry_config_t config;
    bool params_valid;
    uint32_t params_rejected_count;
    bool initialized;
    bool baseline_valid;
    uint32_t reset_count;
    uint32_t rebaseline_count;
    uint32_t sample_count;
    int64_t last_left_motor_counts;
    int64_t last_right_motor_counts;
    uint32_t last_sample_ms;
    float left_wheel_distance_mm;
    float right_wheel_distance_mm;
    float x_mm;
    float y_mm;
    float yaw_rad;
} app_wheel_odometry_t;

void app_wheel_odometry_init(app_wheel_odometry_t *odometry,
                             const app_wheel_odometry_config_t *config);
bool app_wheel_odometry_set_config(app_wheel_odometry_t *odometry,
                                   const app_wheel_odometry_config_t *config);
void app_wheel_odometry_step(app_wheel_odometry_t *odometry,
                             const app_wheel_odometry_input_t *input,
                             app_wheel_odometry_output_t *output);

#endif
