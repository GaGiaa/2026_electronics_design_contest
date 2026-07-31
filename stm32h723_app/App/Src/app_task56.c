#include "app_task56.h"

#include <stddef.h>
#include <string.h>

#include "app_config.h"

static bool app_task56_is_running(app_task56_phase_t phase)
{
    return phase == APP_TASK56_PHASE_RUNNING;
}

static float app_task56_clamp_speed(float speed_mm_s)
{
    if (speed_mm_s < 0.0f) {
        return 0.0f;
    }
    if (speed_mm_s > APP_H723_TASK56_CRUISE_SPEED_MM_S) {
        return APP_H723_TASK56_CRUISE_SPEED_MM_S;
    }
    return speed_mm_s;
}

static float app_task56_speed_for_elapsed(uint32_t elapsed_ms)
{
    const float elapsed_s = (float)elapsed_ms / 1000.0f;
    const float acceleration_time_s =
        APP_H723_TASK56_CRUISE_SPEED_MM_S / APP_H723_TASK56_ACCEL_MM_S2;
    const float braking_time_s =
        APP_H723_TASK56_CRUISE_SPEED_MM_S / APP_H723_TASK56_DECEL_MM_S2;
    const float braking_start_s =
        ((float)APP_H723_TASK56_RUN_TIMEOUT_MS / 1000.0f) - braking_time_s;
    float speed_mm_s;

    if (elapsed_s < acceleration_time_s) {
        speed_mm_s = APP_H723_TASK56_ACCEL_MM_S2 * elapsed_s;
    } else if (elapsed_s < braking_start_s) {
        speed_mm_s = APP_H723_TASK56_CRUISE_SPEED_MM_S;
    } else {
        speed_mm_s = APP_H723_TASK56_CRUISE_SPEED_MM_S -
                     APP_H723_TASK56_DECEL_MM_S2 *
                         (elapsed_s - braking_start_s);
    }
    return app_task56_clamp_speed(speed_mm_s);
}

void app_task56_init(app_task56_state_t *state)
{
    if (state != NULL) {
        (void)memset(state, 0, sizeof(*state));
    }
}

void app_task56_start(app_task56_state_t *state, uint32_t now_ms)
{
    if (state == NULL) {
        return;
    }

    (void)memset(state, 0, sizeof(*state));
    state->phase = APP_TASK56_PHASE_RUNNING;
    state->start_ms = now_ms;
    state->last_step_ms = now_ms;
}

void app_task56_abort(app_task56_state_t *state)
{
    app_task56_init(state);
}

void app_task56_step(app_task56_state_t *state,
                     const app_task56_input_t *input)
{
    uint32_t elapsed_ms;

    if ((state == NULL) || (input == NULL) ||
        !app_task56_is_running(state->phase)) {
        return;
    }

    state->last_step_ms = input->now_ms;
    elapsed_ms = input->now_ms - state->start_ms;
    if (elapsed_ms >= APP_H723_TASK56_RUN_TIMEOUT_MS) {
        state->phase = APP_TASK56_PHASE_STOPPED;
        state->finish_ms = state->start_ms + APP_H723_TASK56_RUN_TIMEOUT_MS;
    }
}

void app_task56_get_output(const app_task56_state_t *state,
                           app_task56_output_t *output)
{
    uint32_t end_ms;
    uint32_t elapsed_ms;

    if ((state == NULL) || (output == NULL)) {
        return;
    }

    (void)memset(output, 0, sizeof(*output));
    output->phase = state->phase;
    output->running = app_task56_is_running(state->phase);
    output->follow_line = output->running;
    output->stop = state->phase == APP_TASK56_PHASE_STOPPED;
    end_ms = output->running ? state->last_step_ms : state->finish_ms;
    elapsed_ms = state->phase == APP_TASK56_PHASE_IDLE ?
                 0U : (end_ms - state->start_ms);
    output->elapsed_ms = elapsed_ms;
    output->base_speed_mm_s = output->running ?
        app_task56_speed_for_elapsed(elapsed_ms) : 0.0f;
}
