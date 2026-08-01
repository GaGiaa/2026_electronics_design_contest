#include "app_speed_profile.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static bool app_speed_profile_config_is_valid(const app_speed_profile_config_t *config)
{
    return config != NULL && isfinite(config->max_speed_mm_s) &&
           isfinite(config->max_accel_mm_s2) && isfinite(config->max_jerk_mm_s3) &&
           config->max_speed_mm_s > 0.0f && config->max_accel_mm_s2 > 0.0f &&
           config->max_jerk_mm_s3 > 0.0f;
}

static float app_speed_profile_clamp(float value, float limit)
{
    if (value > limit) {
        return limit;
    }
    if (value < -limit) {
        return -limit;
    }
    return value;
}

static float app_speed_profile_sign(float value)
{
    return value > 0.0f ? 1.0f : (value < 0.0f ? -1.0f : 0.0f);
}

void app_speed_profile_init(app_speed_profile_t *profile,
                            const app_speed_profile_config_t *config)
{
    if (profile == NULL) {
        return;
    }

    (void)memset(profile, 0, sizeof(*profile));
    if (app_speed_profile_config_is_valid(config)) {
        profile->config = *config;
        profile->valid = true;
    }
}

void app_speed_profile_reset(app_speed_profile_t *profile)
{
    if (profile == NULL) {
        return;
    }

    profile->planned_speed_mm_s = 0.0f;
    profile->planned_accel_mm_s2 = 0.0f;
}

void app_speed_profile_step(app_speed_profile_t *profile,
                            float requested_speed_mm_s,
                            float dt_s,
                            app_speed_profile_output_t *output)
{
    float requested;
    float speed_error;
    float desired_accel;
    float braking_distance;
    float max_accel_delta;
    float next_speed;

    if (output == NULL) {
        return;
    }
    (void)memset(output, 0, sizeof(*output));
    if (profile == NULL || !profile->valid ||
        !app_speed_profile_config_is_valid(&profile->config) ||
        !isfinite(requested_speed_mm_s) || !isfinite(dt_s) || dt_s <= 0.0f) {
        if (profile != NULL) {
            app_speed_profile_reset(profile);
            profile->valid = false;
        }
        return;
    }

    requested = app_speed_profile_clamp(requested_speed_mm_s,
                                        profile->config.max_speed_mm_s);
    speed_error = requested - profile->planned_speed_mm_s;
    braking_distance = profile->planned_accel_mm_s2 * profile->planned_accel_mm_s2 /
                       (2.0f * profile->config.max_jerk_mm_s3);
    if (speed_error == 0.0f ||
        (profile->planned_accel_mm_s2 * speed_error > 0.0f &&
         fabsf(speed_error) <= braking_distance)) {
        desired_accel = 0.0f;
    } else {
        desired_accel = app_speed_profile_sign(speed_error) *
                        profile->config.max_accel_mm_s2;
    }

    max_accel_delta = profile->config.max_jerk_mm_s3 * dt_s;
    if (desired_accel > profile->planned_accel_mm_s2 + max_accel_delta) {
        profile->planned_accel_mm_s2 += max_accel_delta;
    } else if (desired_accel < profile->planned_accel_mm_s2 - max_accel_delta) {
        profile->planned_accel_mm_s2 -= max_accel_delta;
    } else {
        profile->planned_accel_mm_s2 = desired_accel;
    }
    profile->planned_accel_mm_s2 = app_speed_profile_clamp(
        profile->planned_accel_mm_s2, profile->config.max_accel_mm_s2);
    next_speed = profile->planned_speed_mm_s +
                 profile->planned_accel_mm_s2 * dt_s;
    profile->planned_speed_mm_s = app_speed_profile_clamp(
        next_speed, profile->config.max_speed_mm_s);

    output->valid = true;
    output->requested_speed_mm_s = requested;
    output->planned_speed_mm_s = profile->planned_speed_mm_s;
    output->planned_accel_mm_s2 = profile->planned_accel_mm_s2;
}
