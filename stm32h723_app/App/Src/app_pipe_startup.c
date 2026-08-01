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
    snapshot->id3_allowed =
        startup->state == APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION ||
        startup->state == APP_PIPE_STARTUP_STATE_READY;
    snapshot->other_motors_allowed = true;
    snapshot->startup_target_position_deg = APP_H723_PIPE_STARTUP_CALIBRATION_POSITION_DEG;
    snapshot->id3_position_deg = input->id3_position_deg;
    snapshot->id3_output_speed_rpm = input->id3_output_speed_rpm;
    snapshot->startup_position_stable = startup->startup_position_stable;
}

void app_pipe_startup_init(app_pipe_startup_t *startup)
{
    if (startup == NULL) {
        return;
    }

    (void)memset(startup, 0, sizeof(*startup));
    startup->state = APP_PIPE_STARTUP_STATE_WAIT_HOME;
    startup->fault = APP_PIPE_STARTUP_FAULT_NONE;
}

void app_pipe_startup_step(app_pipe_startup_t *startup,
                           const app_pipe_startup_input_t *input,
                           app_pipe_startup_snapshot_t *snapshot)
{
    bool at_startup_position;

    if (snapshot == NULL) {
        return;
    }
    (void)memset(snapshot, 0, sizeof(*snapshot));
    if (startup == NULL || input == NULL) {
        snapshot->state = APP_PIPE_STARTUP_STATE_ID3_LOCKED;
        return;
    }

    if (startup->state == APP_PIPE_STARTUP_STATE_ID3_LOCKED) {
        app_pipe_startup_fill_snapshot(startup, input, snapshot);
        return;
    }

    if (!input->balance_zero_valid) {
        startup->state = APP_PIPE_STARTUP_STATE_WAIT_HOME;
        startup->fault = APP_PIPE_STARTUP_FAULT_NONE;
        startup->move_started_ms = 0U;
        startup->position_stable_started_ms = 0U;
        startup->startup_position_stable = false;
        app_pipe_startup_fill_snapshot(startup, input, snapshot);
        return;
    }

    switch (startup->state) {
    case APP_PIPE_STARTUP_STATE_WAIT_HOME:
        if (app_pipe_startup_move_feedback_is_valid(input)) {
            startup->state = APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION;
            startup->move_started_ms = input->now_ms;
            startup->position_stable_started_ms = 0U;
            startup->startup_position_stable = false;
        }
        break;

    case APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION:
        if (!app_pipe_startup_move_feedback_is_valid(input)) {
            startup->state = APP_PIPE_STARTUP_STATE_ID3_LOCKED;
            startup->fault = APP_PIPE_STARTUP_FAULT_MOVE_FEEDBACK_LOST;
        } else if ((uint32_t)(input->now_ms - startup->move_started_ms) >=
                   APP_H723_PIPE_STARTUP_CALIBRATION_MOVE_TIMEOUT_MS) {
            startup->state = APP_PIPE_STARTUP_STATE_ID3_LOCKED;
            startup->fault = APP_PIPE_STARTUP_FAULT_MOVE_TIMEOUT;
        } else {
            at_startup_position =
                app_pipe_startup_absf(input->id3_position_deg -
                                      APP_H723_PIPE_STARTUP_CALIBRATION_POSITION_DEG) <=
                    APP_H723_PIPE_STARTUP_CALIBRATION_POSITION_TOLERANCE_DEG &&
                app_pipe_startup_absf(input->id3_output_speed_rpm) <=
                    APP_H723_PIPE_STARTUP_CALIBRATION_SPEED_TOLERANCE_RPM;
            if (!at_startup_position) {
                startup->position_stable_started_ms = 0U;
                startup->startup_position_stable = false;
            } else if (!startup->startup_position_stable) {
                startup->startup_position_stable = true;
                startup->position_stable_started_ms = input->now_ms;
            } else if ((uint32_t)(input->now_ms - startup->position_stable_started_ms) >=
                       APP_H723_PIPE_STARTUP_CALIBRATION_SETTLE_MS) {
                startup->state = APP_PIPE_STARTUP_STATE_READY;
                startup->fault = APP_PIPE_STARTUP_FAULT_NONE;
            }
        }
        break;

    case APP_PIPE_STARTUP_STATE_READY:
    default:
        break;
    }

    app_pipe_startup_fill_snapshot(startup, input, snapshot);
}
