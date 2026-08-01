#include <assert.h>
#include <math.h>

#include "app_speed_profile.h"

static app_speed_profile_config_t make_config(void)
{
    return (app_speed_profile_config_t){
        .max_speed_mm_s = 1000.0f,
        .max_accel_mm_s2 = 200.0f,
        .max_jerk_mm_s3 = 1000.0f,
    };
}

static void test_jerk_and_acceleration_are_limited(void)
{
    app_speed_profile_t profile;
    const app_speed_profile_config_t config = make_config();
    app_speed_profile_output_t output = {0};
    float previous_acceleration = 0.0f;
    unsigned int index;

    app_speed_profile_init(&profile, &config);
    for (index = 0U; index < 500U; ++index) {
        app_speed_profile_step(&profile, 1000.0f, 0.001f, &output);
        assert(output.valid);
        assert(fabsf(output.planned_speed_mm_s) <= config.max_speed_mm_s);
        assert(fabsf(output.planned_accel_mm_s2) <= config.max_accel_mm_s2 + 0.001f);
        assert(fabsf(output.planned_accel_mm_s2 - previous_acceleration) <=
               config.max_jerk_mm_s3 * 0.001f + 0.001f);
        previous_acceleration = output.planned_accel_mm_s2;
    }
    assert(output.planned_speed_mm_s > 0.0f);
}

static void test_stop_and_reverse_remain_bounded(void)
{
    app_speed_profile_t profile;
    const app_speed_profile_config_t config = make_config();
    app_speed_profile_output_t output = {0};
    unsigned int index;

    app_speed_profile_init(&profile, &config);
    for (index = 0U; index < 3500U; ++index) {
        app_speed_profile_step(&profile, 500.0f, 0.001f, &output);
    }
    assert(fabsf(output.planned_speed_mm_s - 500.0f) < 1.0f);

    for (index = 0U; index < 6500U; ++index) {
        app_speed_profile_step(&profile, -500.0f, 0.001f, &output);
        assert(fabsf(output.planned_speed_mm_s) <= config.max_speed_mm_s + 0.001f);
        assert(fabsf(output.planned_accel_mm_s2) <= config.max_accel_mm_s2 + 0.001f);
    }
    assert(fabsf(output.planned_speed_mm_s + 500.0f) < 1.0f);
}

static void test_target_crossing_preserves_jerk_limit(void)
{
    app_speed_profile_t profile;
    const app_speed_profile_config_t config = make_config();
    app_speed_profile_output_t output = {0};
    const float dt_s = 0.01f;

    app_speed_profile_init(&profile, &config);
    profile.planned_speed_mm_s = 99.9f;
    profile.planned_accel_mm_s2 = 100.0f;

    app_speed_profile_step(&profile, 100.0f, dt_s, &output);

    assert(output.valid);
    assert(fabsf(output.planned_accel_mm_s2 - 100.0f) <=
           config.max_jerk_mm_s3 * dt_s + 0.001f);
}

static void test_invalid_parameters_reset_profile(void)
{
    app_speed_profile_t profile;
    app_speed_profile_config_t config = make_config();
    app_speed_profile_output_t output = {0};

    config.max_jerk_mm_s3 = 0.0f;
    app_speed_profile_init(&profile, &config);
    app_speed_profile_step(&profile, 500.0f, 0.001f, &output);

    assert(!output.valid);
    assert(fabsf(output.planned_speed_mm_s) < 0.0001f);
    assert(fabsf(output.planned_accel_mm_s2) < 0.0001f);
}

static void test_invalid_step_resets_existing_state(void)
{
    app_speed_profile_t profile;
    const app_speed_profile_config_t config = make_config();
    app_speed_profile_output_t output = {0};

    app_speed_profile_init(&profile, &config);
    app_speed_profile_step(&profile, 500.0f, 0.001f, &output);
    app_speed_profile_step(&profile, 500.0f, 0.0f, &output);

    assert(!output.valid);
    assert(fabsf(profile.planned_speed_mm_s) < 0.0001f);
    assert(fabsf(profile.planned_accel_mm_s2) < 0.0001f);
}

int main(void)
{
    test_jerk_and_acceleration_are_limited();
    test_stop_and_reverse_remain_bounded();
    test_target_crossing_preserves_jerk_limit();
    test_invalid_parameters_reset_profile();
    test_invalid_step_resets_existing_state();
    return 0;
}
