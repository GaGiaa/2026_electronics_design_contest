#ifndef APP_SPEED_PROFILE_H
#define APP_SPEED_PROFILE_H

#include <stdbool.h>

typedef struct {
    float max_speed_mm_s;
    float max_accel_mm_s2;
    float max_jerk_mm_s3;
} app_speed_profile_config_t;

typedef struct {
    app_speed_profile_config_t config;
    float planned_speed_mm_s;
    float planned_accel_mm_s2;
    bool valid;
} app_speed_profile_t;

typedef struct {
    bool valid;
    float requested_speed_mm_s;
    float planned_speed_mm_s;
    float planned_accel_mm_s2;
} app_speed_profile_output_t;

void app_speed_profile_init(app_speed_profile_t *profile,
                            const app_speed_profile_config_t *config);
void app_speed_profile_reset(app_speed_profile_t *profile);
void app_speed_profile_step(app_speed_profile_t *profile,
                            float requested_speed_mm_s,
                            float dt_s,
                            app_speed_profile_output_t *output);

#endif
