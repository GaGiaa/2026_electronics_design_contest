#include "app_wheel_odometry.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "app_config.h"

#define APP_WHEEL_ODOMETRY_PI 3.14159265358979323846f
#define APP_WHEEL_ODOMETRY_ENCODER_COUNTS_PER_REVOLUTION 8192.0f

static bool app_wheel_odometry_config_is_valid(const app_wheel_odometry_config_t *config)
{
    if (config == NULL || !isfinite(config->wheel_diameter_mm) ||
        !isfinite(config->track_width_mm) || !isfinite(config->left_encoder_sign) ||
        !isfinite(config->right_encoder_sign)) {
        return false;
    }
    if (config->wheel_diameter_mm <= 0.0f || config->track_width_mm <= 0.0f) {
        return false;
    }
    return (config->left_encoder_sign == 1.0f || config->left_encoder_sign == -1.0f) &&
           (config->right_encoder_sign == 1.0f || config->right_encoder_sign == -1.0f);
}

static float app_wheel_odometry_counts_to_mm(const app_wheel_odometry_t *odometry,
                                             int64_t counts,
                                             float encoder_sign)
{
    const float wheel_circumference_mm =
        odometry->config.wheel_diameter_mm * APP_WHEEL_ODOMETRY_PI;
    const float counts_per_output_revolution =
        APP_WHEEL_ODOMETRY_ENCODER_COUNTS_PER_REVOLUTION * APP_H723_M2006_GEAR_RATIO;

    return (float)counts * encoder_sign * wheel_circumference_mm /
           counts_per_output_revolution;
}

static float app_wheel_odometry_wrap_degrees(float angle_deg)
{
    angle_deg = fmodf(angle_deg + 180.0f, 360.0f);
    if (angle_deg < 0.0f) {
        angle_deg += 360.0f;
    }
    return angle_deg - 180.0f;
}

static void app_wheel_odometry_publish(const app_wheel_odometry_t *odometry,
                                       const app_wheel_odometry_input_t *input,
                                       bool valid,
                                       float delta_left_mm,
                                       float delta_right_mm,
                                       float delta_distance_mm,
                                       float delta_yaw_deg,
                                       float linear_speed_mm_s,
                                       float angular_speed_deg_s,
                                       app_wheel_odometry_output_t *output)
{
    memset(output, 0, sizeof(*output));
    output->wheel_diameter_mm = odometry->config.wheel_diameter_mm;
    output->track_width_mm = odometry->config.track_width_mm;
    output->left_encoder_sign = odometry->config.left_encoder_sign;
    output->right_encoder_sign = odometry->config.right_encoder_sign;
    output->params_valid = odometry->params_valid;
    output->params_rejected_count = odometry->params_rejected_count;
    output->initialized = odometry->initialized;
    output->valid = valid;
    output->reset_count = odometry->reset_count;
    output->rebaseline_count = odometry->rebaseline_count;
    output->sample_count = odometry->sample_count;
    output->left_motor_counts = input->left_motor_counts;
    output->right_motor_counts = input->right_motor_counts;
    output->left_wheel_distance_mm = odometry->left_wheel_distance_mm;
    output->right_wheel_distance_mm = odometry->right_wheel_distance_mm;
    output->delta_left_mm = delta_left_mm;
    output->delta_right_mm = delta_right_mm;
    output->delta_distance_mm = delta_distance_mm;
    output->delta_yaw_deg = delta_yaw_deg;
    output->x_mm = odometry->x_mm;
    output->y_mm = odometry->y_mm;
    output->yaw_deg_continuous = odometry->yaw_rad * 180.0f / APP_WHEEL_ODOMETRY_PI;
    output->yaw_deg = app_wheel_odometry_wrap_degrees(output->yaw_deg_continuous);
    output->linear_speed_mm_s = linear_speed_mm_s;
    output->angular_speed_deg_s = angular_speed_deg_s;
    output->left_feedback_age_ms = input->left_feedback_age_ms;
    output->right_feedback_age_ms = input->right_feedback_age_ms;
}

void app_wheel_odometry_init(app_wheel_odometry_t *odometry,
                             const app_wheel_odometry_config_t *config)
{
    if (odometry == NULL) {
        return;
    }
    memset(odometry, 0, sizeof(*odometry));
    if (app_wheel_odometry_config_is_valid(config)) {
        odometry->config = *config;
        odometry->params_valid = true;
    }
}

bool app_wheel_odometry_set_config(app_wheel_odometry_t *odometry,
                                   const app_wheel_odometry_config_t *config)
{
    if (odometry == NULL || !app_wheel_odometry_config_is_valid(config)) {
        if (odometry != NULL) {
            odometry->params_valid = false;
            odometry->params_rejected_count++;
        }
        return false;
    }
    odometry->config = *config;
    odometry->params_valid = true;
    return true;
}

void app_wheel_odometry_step(app_wheel_odometry_t *odometry,
                             const app_wheel_odometry_input_t *input,
                             app_wheel_odometry_output_t *output)
{
    const bool feedback_valid = input != NULL && input->left_feedback_valid &&
                                input->right_feedback_valid && odometry != NULL &&
                                odometry->params_valid;
    float delta_left_mm = 0.0f;
    float delta_right_mm = 0.0f;
    float delta_distance_mm = 0.0f;
    float delta_yaw_deg = 0.0f;
    float linear_speed_mm_s = 0.0f;
    float angular_speed_deg_s = 0.0f;

    if (odometry == NULL || input == NULL || output == NULL) {
        return;
    }

    if (input->reset_request) {
        odometry->initialized = false;
        odometry->baseline_valid = false;
        odometry->left_wheel_distance_mm = 0.0f;
        odometry->right_wheel_distance_mm = 0.0f;
        odometry->x_mm = 0.0f;
        odometry->y_mm = 0.0f;
        odometry->yaw_rad = 0.0f;
        odometry->reset_count++;
    }

    if (!feedback_valid) {
        odometry->baseline_valid = false;
        app_wheel_odometry_publish(odometry, input, false, 0.0f, 0.0f, 0.0f,
                                   0.0f, 0.0f, 0.0f, output);
        return;
    }

    if (!odometry->baseline_valid) {
        const bool was_initialized = odometry->initialized;

        odometry->last_left_motor_counts = input->left_motor_counts;
        odometry->last_right_motor_counts = input->right_motor_counts;
        odometry->last_sample_ms = input->now_ms;
        odometry->baseline_valid = true;
        odometry->initialized = true;
        odometry->sample_count++;
        if (was_initialized) {
            odometry->rebaseline_count++;
        }
        app_wheel_odometry_publish(odometry, input, true, 0.0f, 0.0f, 0.0f, 0.0f,
                                   0.0f, 0.0f, output);
        return;
    }

    delta_left_mm = app_wheel_odometry_counts_to_mm(
        odometry, input->left_motor_counts - odometry->last_left_motor_counts,
        odometry->config.left_encoder_sign);
    delta_right_mm = app_wheel_odometry_counts_to_mm(
        odometry, input->right_motor_counts - odometry->last_right_motor_counts,
        odometry->config.right_encoder_sign);
    delta_distance_mm = (delta_left_mm + delta_right_mm) * 0.5f;
    {
        const float delta_yaw_rad =
            (delta_right_mm - delta_left_mm) / odometry->config.track_width_mm;
        const float midpoint_yaw_rad = odometry->yaw_rad + delta_yaw_rad * 0.5f;

        odometry->x_mm += delta_distance_mm * cosf(midpoint_yaw_rad);
        odometry->y_mm += delta_distance_mm * sinf(midpoint_yaw_rad);
        odometry->yaw_rad += delta_yaw_rad;
        delta_yaw_deg = delta_yaw_rad * 180.0f / APP_WHEEL_ODOMETRY_PI;
    }

    if (input->now_ms > odometry->last_sample_ms) {
        const float dt_s = (float)(input->now_ms - odometry->last_sample_ms) / 1000.0f;
        linear_speed_mm_s = delta_distance_mm / dt_s;
        angular_speed_deg_s = delta_yaw_deg / dt_s;
    }
    odometry->last_left_motor_counts = input->left_motor_counts;
    odometry->last_right_motor_counts = input->right_motor_counts;
    odometry->last_sample_ms = input->now_ms;
    odometry->left_wheel_distance_mm += delta_left_mm;
    odometry->right_wheel_distance_mm += delta_right_mm;
    odometry->sample_count++;
    app_wheel_odometry_publish(odometry, input, true, delta_left_mm, delta_right_mm,
                               delta_distance_mm, delta_yaw_deg, linear_speed_mm_s,
                               angular_speed_deg_s, output);
}
