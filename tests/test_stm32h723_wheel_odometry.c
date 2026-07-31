#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "app_wheel_odometry.h"

#define MOTOR_COUNTS_PER_OUTPUT_REVOLUTION (8192LL * 36LL)

static void assert_close(float actual, float expected, float tolerance)
{
    assert(fabsf(actual - expected) <= tolerance);
}

static app_wheel_odometry_config_t default_config(void)
{
    const app_wheel_odometry_config_t config = {
        .wheel_diameter_mm = 65.0f,
        .track_width_mm = 205.0f,
        .left_encoder_sign = 1.0f,
        .right_encoder_sign = -1.0f,
    };
    return config;
}

static app_wheel_odometry_input_t valid_input(uint32_t now_ms,
                                              int64_t left_counts,
                                              int64_t right_counts)
{
    const app_wheel_odometry_input_t input = {
        .now_ms = now_ms,
        .left_feedback_valid = true,
        .right_feedback_valid = true,
        .left_feedback_age_ms = 0U,
        .right_feedback_age_ms = 0U,
        .left_motor_counts = left_counts,
        .right_motor_counts = right_counts,
        .reset_request = false,
    };
    return input;
}

static void test_first_valid_sample_only_establishes_baseline(void)
{
    app_wheel_odometry_t odometry;
    app_wheel_odometry_output_t output;
    const app_wheel_odometry_config_t config = default_config();
    const app_wheel_odometry_input_t input = valid_input(10U, 1000, 2000);

    app_wheel_odometry_init(&odometry, &config);
    app_wheel_odometry_step(&odometry, &input, &output);

    assert(output.initialized);
    assert(output.valid);
    assert(output.sample_count == 1U);
    assert_close(output.left_wheel_distance_mm, 0.0f, 0.001f);
    assert_close(output.right_wheel_distance_mm, 0.0f, 0.001f);
    assert_close(output.x_mm, 0.0f, 0.001f);
    assert_close(output.y_mm, 0.0f, 0.001f);
    assert_close(output.yaw_deg, 0.0f, 0.001f);
}

static void test_equal_forward_wheel_motion_produces_forward_distance(void)
{
    app_wheel_odometry_t odometry;
    app_wheel_odometry_output_t output;
    app_wheel_odometry_config_t config = default_config();
    app_wheel_odometry_input_t input = valid_input(0U, 0, 0);
    const float wheel_circumference_mm = 65.0f * 3.14159265358979323846f;

    app_wheel_odometry_init(&odometry, &config);
    app_wheel_odometry_step(&odometry, &input, &output);

    input = valid_input(100U, MOTOR_COUNTS_PER_OUTPUT_REVOLUTION,
                        -MOTOR_COUNTS_PER_OUTPUT_REVOLUTION);
    app_wheel_odometry_step(&odometry, &input, &output);

    assert(output.valid);
    assert_close(output.left_wheel_distance_mm, wheel_circumference_mm, 0.01f);
    assert_close(output.right_wheel_distance_mm, wheel_circumference_mm, 0.01f);
    assert_close(output.delta_distance_mm, wheel_circumference_mm, 0.01f);
    assert_close(output.x_mm, wheel_circumference_mm, 0.01f);
    assert_close(output.y_mm, 0.0f, 0.01f);
    assert_close(output.yaw_deg, 0.0f, 0.01f);
    assert_close(output.linear_speed_mm_s, wheel_circumference_mm * 10.0f, 0.1f);
}

static void test_opposite_wheel_motion_produces_rotation_without_translation(void)
{
    app_wheel_odometry_t odometry;
    app_wheel_odometry_output_t output;
    const app_wheel_odometry_config_t config = default_config();
    app_wheel_odometry_input_t input = valid_input(0U, 0, 0);
    const float wheel_circumference_mm = 65.0f * 3.14159265358979323846f;
    const float expected_yaw_deg =
        (-2.0f * wheel_circumference_mm / 205.0f) *
        (180.0f / 3.14159265358979323846f);

    app_wheel_odometry_init(&odometry, &config);
    app_wheel_odometry_step(&odometry, &input, &output);

    input = valid_input(100U, MOTOR_COUNTS_PER_OUTPUT_REVOLUTION,
                        MOTOR_COUNTS_PER_OUTPUT_REVOLUTION);
    app_wheel_odometry_step(&odometry, &input, &output);

    assert_close(output.delta_distance_mm, 0.0f, 0.01f);
    assert_close(output.x_mm, 0.0f, 0.01f);
    assert_close(output.y_mm, 0.0f, 0.01f);
    assert_close(output.yaw_deg, expected_yaw_deg, 0.01f);
    assert_close(output.angular_speed_deg_s, expected_yaw_deg * 10.0f, 0.1f);
}

static void test_full_in_place_rotation_wraps_heading_but_preserves_turn_count(void)
{
    app_wheel_odometry_t odometry;
    app_wheel_odometry_output_t output;
    const app_wheel_odometry_config_t config = default_config();
    app_wheel_odometry_input_t input = valid_input(0U, 0, 0);
    const int64_t counts_for_one_turn = 930107;

    app_wheel_odometry_init(&odometry, &config);
    app_wheel_odometry_step(&odometry, &input, &output);

    input = valid_input(1000U, -counts_for_one_turn, -counts_for_one_turn);
    app_wheel_odometry_step(&odometry, &input, &output);

    assert_close(output.x_mm, 0.0f, 0.01f);
    assert_close(output.y_mm, 0.0f, 0.01f);
    assert_close(output.yaw_deg, 0.0f, 0.01f);
    assert_close(output.yaw_deg_continuous, 360.0f, 0.02f);
}

static void test_feedback_loss_freezes_and_recovery_rebaselines(void)
{
    app_wheel_odometry_t odometry;
    app_wheel_odometry_output_t output;
    const app_wheel_odometry_config_t config = default_config();
    app_wheel_odometry_input_t input = valid_input(0U, 0, 0);
    float distance_before_loss;

    app_wheel_odometry_init(&odometry, &config);
    app_wheel_odometry_step(&odometry, &input, &output);
    input = valid_input(10U, 1000, -1000);
    app_wheel_odometry_step(&odometry, &input, &output);
    distance_before_loss = output.x_mm;

    input = valid_input(70U, 5000, -5000);
    input.left_feedback_valid = false;
    input.left_feedback_age_ms = 50U;
    app_wheel_odometry_step(&odometry, &input, &output);
    assert(!output.valid);
    assert_close(output.x_mm, distance_before_loss, 0.001f);

    input = valid_input(80U, 6000, -6000);
    app_wheel_odometry_step(&odometry, &input, &output);
    assert(output.valid);
    assert(output.rebaseline_count == 1U);
    assert_close(output.delta_left_mm, 0.0f, 0.001f);
    assert_close(output.delta_right_mm, 0.0f, 0.001f);
    assert_close(output.x_mm, distance_before_loss, 0.001f);

    input = valid_input(90U, 7000, -7000);
    app_wheel_odometry_step(&odometry, &input, &output);
    assert(output.valid);
    assert(output.delta_distance_mm > 0.0f);
    assert(output.x_mm > distance_before_loss);
}

static void test_watch_reset_clears_pose_and_rebaselines(void)
{
    app_wheel_odometry_t odometry;
    app_wheel_odometry_output_t output;
    const app_wheel_odometry_config_t config = default_config();
    app_wheel_odometry_input_t input = valid_input(0U, 0, 0);

    app_wheel_odometry_init(&odometry, &config);
    app_wheel_odometry_step(&odometry, &input, &output);
    input = valid_input(10U, 1000, -1000);
    app_wheel_odometry_step(&odometry, &input, &output);
    assert(output.x_mm > 0.0f);

    input = valid_input(20U, 1000, -1000);
    input.reset_request = true;
    app_wheel_odometry_step(&odometry, &input, &output);
    assert(output.reset_count == 1U);
    assert_close(output.x_mm, 0.0f, 0.001f);
    assert_close(output.y_mm, 0.0f, 0.001f);
    assert_close(output.yaw_deg, 0.0f, 0.001f);

    input = valid_input(30U, 2000, -2000);
    app_wheel_odometry_step(&odometry, &input, &output);
    assert(output.x_mm > 0.0f);
}

static void test_invalid_configuration_is_rejected(void)
{
    app_wheel_odometry_t odometry;
    app_wheel_odometry_output_t output;
    app_wheel_odometry_config_t config = default_config();
    const app_wheel_odometry_input_t input = valid_input(0U, 0, 0);

    app_wheel_odometry_init(&odometry, &config);
    config.track_width_mm = 0.0f;
    assert(!app_wheel_odometry_set_config(&odometry, &config));
    config = default_config();
    config.right_encoder_sign = 0.5f;
    assert(!app_wheel_odometry_set_config(&odometry, &config));
    app_wheel_odometry_step(&odometry, &input, &output);
    assert(!output.params_valid);
    assert(!output.valid);
    assert(output.params_rejected_count == 2U);
}

int main(void)
{
    test_first_valid_sample_only_establishes_baseline();
    test_equal_forward_wheel_motion_produces_forward_distance();
    test_opposite_wheel_motion_produces_rotation_without_translation();
    test_full_in_place_rotation_wraps_heading_but_preserves_turn_count();
    test_feedback_loss_freezes_and_recovery_rebaselines();
    test_watch_reset_clears_pose_and_rebaselines();
    test_invalid_configuration_is_rejected();
    return 0;
}
