#include "app_pipe_startup.h"

#include <string.h>

static void app_pipe_startup_fill_snapshot(const app_pipe_startup_t *startup,
                                           app_pipe_startup_snapshot_t *snapshot)
{
    snapshot->state = startup->state;
    snapshot->calibration_valid = startup->calibration_valid;
    snapshot->captured_pitch_deg = startup->captured_pitch_deg;
    snapshot->id3_allowed = (startup->state == APP_PIPE_STARTUP_STATE_READY);
    snapshot->other_motors_allowed = true;
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
            startup->state = APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED;
        }
    } else if (startup->state == APP_PIPE_STARTUP_STATE_READY &&
               !input->balance_zero_valid) {
        startup->state = APP_PIPE_STARTUP_STATE_WAIT_HOME;
        startup->calibration_valid = false;
    } else if (startup->state == APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED) {
        if ((rising_buttons & APP_PIPE_STARTUP_BUTTON_SKIP) != 0U) {
            startup->state = APP_PIPE_STARTUP_STATE_ID3_LOCKED;
        } else if ((rising_buttons & APP_PIPE_STARTUP_BUTTON_CAPTURE) != 0U &&
                   input->imu_pitch_valid && input->imu_pitch_fresh) {
            startup->captured_pitch_deg = input->imu_pitch_deg;
            startup->calibration_valid = true;
            startup->state = APP_PIPE_STARTUP_STATE_READY;
        }
    }

    app_pipe_startup_fill_snapshot(startup, snapshot);
}
