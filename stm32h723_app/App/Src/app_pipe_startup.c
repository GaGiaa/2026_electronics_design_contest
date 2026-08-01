#include "app_pipe_startup.h"

#include "app_config.h"

#include <math.h>
#include <string.h>

static float app_pipe_startup_absf(float value)
{
    return value < 0.0f ? -value : value;
}

static bool app_pipe_startup_move_feedback_is_valid(const app_pipe_startup_input_t *input)
{
    return input->id3_feedback_valid && isfinite(input->id3_position_deg) &&
           isfinite(input->id3_output_speed_rpm);
}

static void app_pipe_startup_fill_snapshot(const app_pipe_startup_t *startup,
                                           const app_pipe_startup_input_t *input,
                                           app_pipe_startup_snapshot_t *snapshot)
{
    snapshot->state = startup->state;
    snapshot->fault = startup->fault;
    snapshot->calibration_valid = startup->calibration_valid;
    snapshot->captured_pitch_deg = startup->captured_pitch_deg;
    snapshot->id3_allowed =
        startup->state == APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION ||
        startup->state == APP_PIPE_STARTUP_STATE_READY;
    snapshot->other_motors_allowed = true;
    snapshot->calibration_target_position_deg =
        APP_H723_PIPE_STARTUP_CALIBRATION_POSITION_DEG;
    snapshot->calibration_position_stable = startup->calibration_position_stable;
    snapshot->id3_position_deg = input->id3_position_deg;
    snapshot->id3_output_speed_rpm = input->id3_output_speed_rpm;
}

void app_pipe_startup_init(app_pipe_startup_t *startup)
{
    if (startup == NULL) {
        return;
    }
    memset(startup, 0, sizeof(*startup));
    startup->state = APP_PIPE_STARTUP_STATE_WAIT_HOME;
}

void app_pipe_startup_step(app_pipe_startup_t *startup,
                           const app_pipe_startup_input_t *input,
                           app_pipe_startup_snapshot_t *snapshot)
{
    uint32_t rising_buttons;

    if (startup == NULL || input == NULL || snapshot == NULL) {
        return;
    }

    rising_buttons = input->buttons & ~startup->previous_buttons;
    startup->previous_buttons = input->buttons;

    if (startup->state == APP_PIPE_STARTUP_STATE_WAIT_HOME) {
        if (input->balance_zero_valid) {
            startup->state = APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION;
            startup->fault = APP_PIPE_STARTUP_FAULT_NONE;
            startup->move_started_ms = input->now_ms;
            startup->calibration_position_stable = false;
        }
    } else if (startup->state == APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION) {
        if (!input->balance_zero_valid) {
            startup->state = APP_PIPE_STARTUP_STATE_ID3_LOCKED;
            startup->fault = APP_PIPE_STARTUP_FAULT_MOVE_ZERO_LOST;
        } else if (!app_pipe_startup_move_feedback_is_valid(input)) {
            startup->state = APP_PIPE_STARTUP_STATE_ID3_LOCKED;
            startup->fault = APP_PIPE_STARTUP_FAULT_MOVE_FEEDBACK_LOST;
        } else if ((uint32_t)(input->now_ms - startup->move_started_ms) >=
                   APP_H723_PIPE_STARTUP_CALIBRATION_MOVE_TIMEOUT_MS) {
            startup->state = APP_PIPE_STARTUP_STATE_ID3_LOCKED;
            startup->fault = APP_PIPE_STARTUP_FAULT_MOVE_TIMEOUT;
        } else if (app_pipe_startup_absf(input->id3_position_deg -
                                          APP_H723_PIPE_STARTUP_CALIBRATION_POSITION_DEG) <=
                       APP_H723_PIPE_STARTUP_CALIBRATION_POSITION_TOLERANCE_DEG &&
                   app_pipe_startup_absf(input->id3_output_speed_rpm) <=
                       APP_H723_PIPE_STARTUP_CALIBRATION_SPEED_TOLERANCE_RPM) {
            if (!startup->calibration_position_stable) {
                startup->position_stable_started_ms = input->now_ms;
                startup->calibration_position_stable = true;
            } else if ((uint32_t)(input->now_ms - startup->position_stable_started_ms) >=
                       APP_H723_PIPE_STARTUP_CALIBRATION_SETTLE_MS) {
                startup->state = APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED;
                startup->calibration_position_stable = false;
            }
        } else {
            startup->calibration_position_stable = false;
        }
    } else if (startup->state == APP_PIPE_STARTUP_STATE_READY &&
               !input->balance_zero_valid) {
        startup->state = APP_PIPE_STARTUP_STATE_WAIT_HOME;
        startup->calibration_valid = false;
    } else if (startup->state == APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED) {
        if ((rising_buttons & APP_PIPE_STARTUP_BUTTON_SKIP) != 0U) {
            startup->state = APP_PIPE_STARTUP_STATE_ID3_LOCKED;
            startup->fault = APP_PIPE_STARTUP_FAULT_USER_SKIPPED;
        } else if ((rising_buttons & APP_PIPE_STARTUP_BUTTON_CAPTURE) != 0U &&
                   input->imu_pitch_valid && input->imu_pitch_fresh) {
            startup->captured_pitch_deg = input->imu_pitch_deg;
            startup->calibration_valid = true;
            startup->state = APP_PIPE_STARTUP_STATE_READY;
        }
    }

    app_pipe_startup_fill_snapshot(startup, input, snapshot);
}
