#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "algorithms/course_following/course_following.h"

static board_grayscale_snapshot_t make_snapshot(int32_t line_error,
                                                 uint32_t line_strength,
                                                 uint8_t black_mask,
                                                 uint8_t black_count)
{
    board_grayscale_snapshot_t snapshot = {0};

    snapshot.line_error = line_error;
    snapshot.line_strength = line_strength;
    snapshot.black_mask = black_mask;
    snapshot.black_count = black_count;
    return snapshot;
}

static course_following_input_t make_input(
    const board_grayscale_snapshot_t *grayscale, bool yaw_valid,
    float yaw_deg, uint32_t now_ms)
{
    course_following_input_t input = {
        .grayscale = grayscale,
        .yaw_valid = yaw_valid,
        .yaw_deg = yaw_deg,
        .now_ms = now_ms,
    };
    return input;
}

static void assert_stopped(const course_following_output_t *output)
{
    uint32_t wheel;

    for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
        assert(fabsf(output->wheel_targets_mm_per_s[wheel]) < 0.001f);
    }
}

static void test_invalid_yaw_and_all_black_stop(void)
{
    course_following_state_t state;
    course_following_output_t output;
    board_grayscale_snapshot_t line = make_snapshot(0, 4095U, 0x18U, 2U);
    board_grayscale_snapshot_t all_black =
        make_snapshot(0, 32760U, 0xFFU, 8U);
    course_following_input_t input;

    course_following_init(&state);
    input = make_input(&line, false, 0.0f, 0U);
    course_following_step(&state, &input, &output);
    assert_stopped(&output);

    input = make_input(&all_black, true, 0.0f, 10U);
    course_following_step(&state, &input, &output);
    assert(output.all_black_stop);
    assert_stopped(&output);
}

static void test_wireless_heading_and_conservative_corner_speeds(void)
{
    course_following_state_t state;
    course_following_output_t output;
    board_grayscale_snapshot_t wireless = make_snapshot(0, 0U, 0U, 0U);
    board_grayscale_snapshot_t left_corner =
        make_snapshot(3000, 12000U, 0x03U, 2U);
    course_following_input_t input;
    uint32_t sample;

    course_following_init(&state);
    for (sample = 0U; sample < 5U; ++sample) {
        input = make_input(&wireless, true, 0.0f, sample * 10U);
        course_following_step(&state, &input, &output);
    }
    assert(output.heading_hold);
    assert(fabsf(output.heading_target_deg) < 0.001f);
    assert(fabsf(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT] -
                 150.0f) < 0.001f);

    input = make_input(&left_corner, true, 0.0f, 60U);
    course_following_step(&state, &input, &output);
    assert(output.right_angle_turn_active);
    assert(fabsf(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT] +
                 200.0f) < 0.001f);
    assert(fabsf(output.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_RIGHT] -
                 300.0f) < 0.001f);
}

static void test_wireless_sequence_and_short_dropout(void)
{
    course_following_state_t state;
    course_following_output_t output;
    board_grayscale_snapshot_t wireless = make_snapshot(0, 0U, 0U, 0U);
    board_grayscale_snapshot_t center = make_snapshot(0, 4095U, 0x18U, 2U);
    course_following_input_t input;
    uint32_t sample;

    course_following_init(&state);
    for (sample = 0U; sample < 4U; ++sample) {
        input = make_input(&wireless, true, 0.0f, sample * 10U);
        course_following_step(&state, &input, &output);
        assert(!output.heading_hold);
    }
    input = make_input(&wireless, true, 0.0f, 40U);
    course_following_step(&state, &input, &output);
    assert(output.heading_hold);
    assert(fabsf(output.heading_target_deg) < 0.001f);

    input = make_input(&center, true, 0.0f, 50U);
    course_following_step(&state, &input, &output);
    assert(!output.heading_hold);
    for (sample = 0U; sample < 5U; ++sample) {
        input = make_input(&wireless, true, 0.0f, 60U + sample * 10U);
        course_following_step(&state, &input, &output);
    }
    assert(output.heading_hold);
    assert(fabsf(output.heading_target_deg - 175.0f) < 0.001f);
}

static void test_right_angle_exit_and_rearm_delay(void)
{
    course_following_state_t state;
    course_following_output_t output;
    board_grayscale_snapshot_t left_corner =
        make_snapshot(0, 12000U, 0x03U, 2U);
    board_grayscale_snapshot_t center = make_snapshot(0, 4095U, 0x18U, 2U);
    course_following_input_t input;
    uint32_t sample;

    course_following_init(&state);
    input = make_input(&left_corner, true, 0.0f, 0U);
    course_following_step(&state, &input, &output);
    assert(output.right_angle_turn_active);

    input = make_input(&center, true, 0.0f, 490U);
    course_following_step(&state, &input, &output);
    assert(output.right_angle_turn_active);
    for (sample = 0U; sample < 3U; ++sample) {
        input = make_input(&center, true, 0.0f, 500U + sample * 10U);
        course_following_step(&state, &input, &output);
    }
    assert(!output.right_angle_turn_active);

    input = make_input(&left_corner, true, 0.0f, 590U);
    course_following_step(&state, &input, &output);
    assert(!output.right_angle_turn_active);
    input = make_input(&left_corner, true, 0.0f, 620U);
    course_following_step(&state, &input, &output);
    assert(output.right_angle_turn_active);
}

int main(void)
{
    test_invalid_yaw_and_all_black_stop();
    test_wireless_heading_and_conservative_corner_speeds();
    test_wireless_sequence_and_short_dropout();
    test_right_angle_exit_and_rearm_delay();
    return 0;
}
