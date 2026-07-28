#include "course_following.h"

#include <math.h>
#include <stddef.h>

#include "config/course_following_config.h"

#define COURSE_FOLLOWING_DT_S 0.01f

static const PID_Position_Param_Config g_line_pid_params = {
    .kp = COURSE_FOLLOWING_LINE_KP,
    .ki = 0.0f,
    .kd = 0.0f,
    .output_limit = COURSE_FOLLOWING_LINE_OUTPUT_LIMIT_MM_PER_S,
    .deadband = 0.0f,
};

static const PID_Position_Param_Config g_heading_pid_params = {
    .kp = COURSE_FOLLOWING_HEADING_KP,
    .ki = 0.0f,
    .kd = 0.0f,
    .output_limit = COURSE_FOLLOWING_HEADING_OUTPUT_LIMIT_MM_PER_S,
    .deadband = 0.0f,
};

static float normalize_heading(float heading_deg)
{
    while (heading_deg >= 180.0f) {
        heading_deg -= 360.0f;
    }
    while (heading_deg < -180.0f) {
        heading_deg += 360.0f;
    }
    return heading_deg;
}

static float shortest_heading_error(float target_deg, float current_deg)
{
    return normalize_heading(target_deg - current_deg);
}

static float clamp_speed(float speed_mm_per_s)
{
    if (speed_mm_per_s > COURSE_FOLLOWING_MAX_SPEED_MM_PER_S) {
        return COURSE_FOLLOWING_MAX_SPEED_MM_PER_S;
    }
    if (speed_mm_per_s < -COURSE_FOLLOWING_MAX_SPEED_MM_PER_S) {
        return -COURSE_FOLLOWING_MAX_SPEED_MM_PER_S;
    }
    return speed_mm_per_s;
}

static void set_targets(course_following_output_t *output, float left_speed,
                        float right_speed)
{
    uint32_t wheel;

    if (output == NULL) {
        return;
    }
    for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
        output->wheel_targets_mm_per_s[wheel] = 0.0f;
    }
    output->wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT] =
        clamp_speed(left_speed);
    output->wheel_targets_mm_per_s[BOARD_MOTOR_REAR_LEFT] =
        clamp_speed(left_speed);
    output->wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_RIGHT] =
        clamp_speed(right_speed);
    output->wheel_targets_mm_per_s[BOARD_MOTOR_REAR_RIGHT] =
        clamp_speed(right_speed);
}

static bool line_is_present(const board_grayscale_snapshot_t *snapshot)
{
    return snapshot->black_count != 0U ||
           snapshot->line_strength >= COURSE_FOLLOWING_LINE_STRENGTH_MIN;
}

static bool right_angle_candidate(const board_grayscale_snapshot_t *snapshot,
                                  int8_t *direction)
{
    bool left_edge_black;
    bool right_edge_black;

    if (snapshot->black_count < COURSE_FOLLOWING_RIGHT_ANGLE_MIN_BLACK_COUNT) {
        return false;
    }
    left_edge_black = (snapshot->black_mask &
                       COURSE_FOLLOWING_RIGHT_ANGLE_LEFT_EDGE_MASK) ==
                      COURSE_FOLLOWING_RIGHT_ANGLE_LEFT_EDGE_MASK;
    right_edge_black = (snapshot->black_mask &
                        COURSE_FOLLOWING_RIGHT_ANGLE_RIGHT_EDGE_MASK) ==
                       COURSE_FOLLOWING_RIGHT_ANGLE_RIGHT_EDGE_MASK;
    if (left_edge_black == right_edge_black) {
        return false;
    }
    *direction = left_edge_black ? -1 : 1;
    return true;
}

static bool centered_after_right_angle(const board_grayscale_snapshot_t *snapshot)
{
    return (snapshot->black_mask & COURSE_FOLLOWING_RIGHT_ANGLE_CENTER_MASK) != 0U &&
           snapshot->black_count <= 3U;
}

static float heading_target_for_segment(uint8_t segment_index)
{
    float correction_deg = (float)(segment_index / 2U) *
                           COURSE_FOLLOWING_STRAIGHT_HEADING_STEP_DEG;

    return normalize_heading((segment_index & 1U) == 0U ?
        COURSE_FOLLOWING_STRAIGHT_HEADING_A_DEG - correction_deg :
        COURSE_FOLLOWING_STRAIGHT_HEADING_B_DEG - correction_deg);
}

static void advance_straight_segment(course_following_state_t *state)
{
    ++state->straight_segment_index;
    if (state->straight_segment_index >=
        COURSE_FOLLOWING_STRAIGHT_HEADING_SEQUENCE_LENGTH) {
        state->straight_segment_index = 0U;
    }
    state->heading_target_deg =
        heading_target_for_segment(state->straight_segment_index);
}

static void reset_right_angle(course_following_state_t *state)
{
    state->right_angle_turn_active = false;
    state->right_angle_turn_direction = 0;
    state->right_angle_turn_started_ms = 0U;
    state->right_angle_center_samples = 0U;
}

void course_following_reset(course_following_state_t *state)
{
    if (state == NULL) {
        return;
    }
    PID_Position_Reset(&state->line_pid);
    PID_Position_Reset(&state->heading_pid);
    state->heading_target_deg = 0.0f;
    state->heading_target_initialized = false;
    state->heading_hold_active = false;
    state->wireless_confirm_samples = 0U;
    state->straight_segment_index = 0U;
    reset_right_angle(state);
    state->right_angle_rearm_until_ms = 0U;
}

void course_following_init(course_following_state_t *state)
{
    if (state == NULL) {
        return;
    }
    PID_Position_Init(&state->line_pid, &g_line_pid_params,
                      COURSE_FOLLOWING_DT_S);
    PID_Position_Init(&state->heading_pid, &g_heading_pid_params,
                      COURSE_FOLLOWING_DT_S);
    course_following_reset(state);
}

void course_following_step(course_following_state_t *state,
                           const course_following_input_t *input,
                           course_following_output_t *output)
{
    const board_grayscale_snapshot_t *snapshot;
    bool line_present;
    bool wireless_mode;
    float correction;

    if (output == NULL) {
        return;
    }
    *output = (course_following_output_t){0};
    if ((state == NULL) || (input == NULL) || (input->grayscale == NULL)) {
        return;
    }
    snapshot = input->grayscale;
    output->yaw_valid = input->yaw_valid;
    output->heading_target_deg = state->heading_target_deg;

    if (!input->yaw_valid) {
        course_following_reset(state);
        return;
    }
    if (snapshot->black_count == BOARD_GRAYSCALE_CHANNEL_COUNT &&
        snapshot->black_mask == 0xFFU) {
        course_following_reset(state);
        output->all_black_stop = true;
        return;
    }

    line_present = line_is_present(snapshot);
    if (line_present) {
        state->wireless_confirm_samples = 0U;
    } else if (state->wireless_confirm_samples <
               COURSE_FOLLOWING_WIRELESS_CONFIRM_SAMPLES) {
        ++state->wireless_confirm_samples;
    }
    wireless_mode = !line_present &&
                    (state->heading_hold_active ||
                     state->wireless_confirm_samples >=
                         COURSE_FOLLOWING_WIRELESS_CONFIRM_SAMPLES);
    if (wireless_mode) {
        PID_Position_Reset(&state->line_pid);
        reset_right_angle(state);
        if (!state->heading_target_initialized) {
            state->heading_target_deg = heading_target_for_segment(0U);
            state->heading_target_initialized = true;
        } else if (!state->heading_hold_active) {
            advance_straight_segment(state);
        }
        state->heading_hold_active = true;
        correction = PID_Position_Calc(
            &state->heading_pid, 0.0f,
            -shortest_heading_error(state->heading_target_deg, input->yaw_deg));
        output->line_lost = true;
        output->heading_hold = true;
        output->heading_target_deg = state->heading_target_deg;
        output->heading_error_deg =
            shortest_heading_error(state->heading_target_deg, input->yaw_deg);
        output->heading_pid_output = correction;
        set_targets(output, COURSE_FOLLOWING_BASE_SPEED_MM_PER_S - correction,
                    COURSE_FOLLOWING_BASE_SPEED_MM_PER_S + correction);
        return;
    }

    state->heading_hold_active = false;
    PID_Position_Reset(&state->heading_pid);
    if (state->right_angle_turn_active) {
        uint32_t elapsed_ms = (uint32_t)(input->now_ms -
                                         state->right_angle_turn_started_ms);

        if (elapsed_ms >= COURSE_FOLLOWING_RIGHT_ANGLE_MIN_TURN_TIME_MS &&
            centered_after_right_angle(snapshot)) {
            ++state->right_angle_center_samples;
        } else {
            state->right_angle_center_samples = 0U;
        }
        if (state->right_angle_center_samples >=
                COURSE_FOLLOWING_RIGHT_ANGLE_CENTER_CONFIRM_SAMPLES ||
            elapsed_ms >= COURSE_FOLLOWING_RIGHT_ANGLE_MAX_TURN_TIME_MS) {
            reset_right_angle(state);
            state->right_angle_rearm_until_ms = input->now_ms +
                COURSE_FOLLOWING_RIGHT_ANGLE_REARM_TIME_MS;
        } else {
            output->right_angle_turn_active = true;
            output->right_angle_turn_direction = state->right_angle_turn_direction;
            output->right_angle_turn_elapsed_ms = elapsed_ms;
            if (state->right_angle_turn_direction < 0) {
                set_targets(output, COURSE_FOLLOWING_RIGHT_ANGLE_INNER_SPEED_MM_PER_S,
                            COURSE_FOLLOWING_RIGHT_ANGLE_OUTER_SPEED_MM_PER_S);
            } else {
                set_targets(output, COURSE_FOLLOWING_RIGHT_ANGLE_OUTER_SPEED_MM_PER_S,
                            COURSE_FOLLOWING_RIGHT_ANGLE_INNER_SPEED_MM_PER_S);
            }
            return;
        }
    }
    if ((int32_t)(input->now_ms - state->right_angle_rearm_until_ms) >= 0) {
        int8_t direction;

        if (right_angle_candidate(snapshot, &direction)) {
            state->right_angle_turn_active = true;
            state->right_angle_turn_direction = direction;
            state->right_angle_turn_started_ms = input->now_ms;
            state->right_angle_center_samples = 0U;
            output->right_angle_turn_active = true;
            output->right_angle_turn_direction = direction;
            if (direction < 0) {
                set_targets(output, COURSE_FOLLOWING_RIGHT_ANGLE_INNER_SPEED_MM_PER_S,
                            COURSE_FOLLOWING_RIGHT_ANGLE_OUTER_SPEED_MM_PER_S);
            } else {
                set_targets(output, COURSE_FOLLOWING_RIGHT_ANGLE_OUTER_SPEED_MM_PER_S,
                            COURSE_FOLLOWING_RIGHT_ANGLE_INNER_SPEED_MM_PER_S);
            }
            return;
        }
    }

    output->line_lost = !line_present;
    correction = PID_Position_Calc(&state->line_pid, 0.0f,
                                   (float)snapshot->line_error);
    output->line_pid_output = correction;
    output->heading_target_deg = state->heading_target_deg;
    set_targets(output, COURSE_FOLLOWING_BASE_SPEED_MM_PER_S - correction,
                COURSE_FOLLOWING_BASE_SPEED_MM_PER_S + correction);
}
