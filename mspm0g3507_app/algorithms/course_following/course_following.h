#ifndef COURSE_FOLLOWING_H
#define COURSE_FOLLOWING_H

#include <stdbool.h>
#include <stdint.h>

#include "algorithms/pid/pid.h"
#include "drivers/grayscale/board_grayscale.h"
#include "drivers/motor/board_motor.h"

typedef struct {
    PID_Position line_pid;
    PID_Position heading_pid;
    float heading_target_deg;
    bool heading_target_initialized;
    bool heading_hold_active;
    uint8_t wireless_confirm_samples;
    uint8_t straight_segment_index;
    bool right_angle_turn_active;
    int8_t right_angle_turn_direction;
    uint32_t right_angle_turn_started_ms;
    uint8_t right_angle_center_samples;
    uint32_t right_angle_rearm_until_ms;
} course_following_state_t;

typedef struct {
    const board_grayscale_snapshot_t *grayscale;
    bool yaw_valid;
    float yaw_deg;
    uint32_t now_ms;
} course_following_input_t;

typedef struct {
    float wheel_targets_mm_per_s[BOARD_MOTOR_COUNT];
    bool line_lost;
    bool all_black_stop;
    bool heading_hold;
    bool yaw_valid;
    float heading_target_deg;
    float heading_error_deg;
    float heading_pid_output;
    float line_pid_output;
    bool right_angle_turn_active;
    int8_t right_angle_turn_direction;
    uint32_t right_angle_turn_elapsed_ms;
} course_following_output_t;

void course_following_init(course_following_state_t *state);
void course_following_reset(course_following_state_t *state);
void course_following_step(course_following_state_t *state,
                           const course_following_input_t *input,
                           course_following_output_t *output);

#endif
