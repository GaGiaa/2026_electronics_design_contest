#include "app_task2.h"

#include <stddef.h>
#include <string.h>

#include "app_config.h"

static bool app_task2_is_running(app_task2_phase_t phase)
{
    return phase == APP_TASK2_PHASE_RUNNING;
}

void app_task2_init(app_task2_state_t *state)
{
    if (state != NULL) {
        (void)memset(state, 0, sizeof(*state));
    }
}

void app_task2_start(app_task2_state_t *state, uint32_t now_ms)
{
    if (state == NULL) {
        return;
    }

    (void)memset(state, 0, sizeof(*state));
    state->phase = APP_TASK2_PHASE_RUNNING;
    state->start_ms = now_ms;
    state->last_step_ms = now_ms;
}

void app_task2_abort(app_task2_state_t *state)
{
    app_task2_init(state);
}

void app_task2_step(app_task2_state_t *state, const app_task2_input_t *input)
{
    if ((state == NULL) || (input == NULL) || !app_task2_is_running(state->phase)) {
        return;
    }

    state->last_step_ms = input->now_ms;
    if ((input->now_ms - state->start_ms) >= APP_H723_TASK2_STARTUP_IGNORE_STOP_MS &&
        input->black_count >= APP_H723_TASK2_STOP_BLACK_COUNT) {
        state->phase = APP_TASK2_PHASE_STOPPED;
        state->finish_ms = input->now_ms;
    }
}

void app_task2_get_output(const app_task2_state_t *state, app_task2_output_t *output)
{
    uint32_t end_ms;

    if ((state == NULL) || (output == NULL)) {
        return;
    }

    (void)memset(output, 0, sizeof(*output));
    output->phase = state->phase;
    output->running = app_task2_is_running(state->phase);
    output->follow_line = output->running;
    output->stop = state->phase == APP_TASK2_PHASE_STOPPED;
    output->base_speed_mm_s = output->running ? APP_H723_TASK2_SPEED_MM_S : 0.0f;
    end_ms = output->running ? state->last_step_ms : state->finish_ms;
    output->elapsed_ms = state->phase == APP_TASK2_PHASE_IDLE ?
                         0U : (end_ms - state->start_ms);
}
