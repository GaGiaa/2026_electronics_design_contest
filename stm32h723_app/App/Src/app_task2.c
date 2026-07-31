#include "app_task2.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "app_config.h"

static bool is_running(app_task2_phase_t phase)
{
    return (phase == APP_TASK2_PHASE_DEPART) ||
           (phase == APP_TASK2_PHASE_CRUISE) ||
           (phase == APP_TASK2_PHASE_APPROACH);
}

static void enter_fault(app_task2_state_t *state, app_task2_fault_t fault,
                        uint32_t now_ms)
{
    state->phase = APP_TASK2_PHASE_FAULT;
    state->fault = fault;
    state->finish_ms = now_ms;
}

void app_task2_init(app_task2_state_t *state)
{
    if (state != NULL) {
        (void)memset(state, 0, sizeof(*state));
    }
}

void app_task2_start(app_task2_state_t *state, uint32_t now_ms)
{
    if (state == NULL) { return; }
    (void)memset(state, 0, sizeof(*state));
    state->phase = APP_TASK2_PHASE_DEPART;
    state->start_ms = now_ms;
    state->last_step_ms = now_ms;
}

void app_task2_abort(app_task2_state_t *state)
{
    app_task2_init(state);
}

void app_task2_step(app_task2_state_t *state, const app_task2_input_t *input)
{
    uint32_t dt_ms;
    bool stop_mark;
    bool line_valid;

    if ((state == NULL) || (input == NULL) || !is_running(state->phase)) { return; }
    dt_ms = input->now_ms - state->last_step_ms;
    state->last_step_ms = input->now_ms;

    if ((input->now_ms - state->start_ms) >= APP_H723_TASK2_RUN_TIMEOUT_MS) {
        enter_fault(state, APP_TASK2_FAULT_RUN_TIMEOUT, input->now_ms);
        return;
    }
    if (input->adc_timeout_mask != 0U) {
        enter_fault(state, APP_TASK2_FAULT_ADC_TIMEOUT, input->now_ms);
        return;
    }
    if (!input->left_feedback_fresh || !input->right_feedback_fresh) {
        enter_fault(state, APP_TASK2_FAULT_FEEDBACK_STALE, input->now_ms);
        return;
    }

    state->distance_mm += 0.5f *
        (fabsf(input->left_speed_mm_s) + fabsf(input->right_speed_mm_s)) *
        ((float)dt_ms / 1000.0f);

    line_valid = input->line_strength >= APP_H723_LINE_FOLLOW_LINE_STRENGTH_MIN;
    if (!line_valid) {
        if (!state->line_lost_timer_active) {
            state->line_lost_timer_active = true;
            state->line_lost_since_ms = input->now_ms;
        } else if ((input->now_ms - state->line_lost_since_ms) >=
                   APP_H723_TASK2_LINE_LOST_TIMEOUT_MS) {
            enter_fault(state, APP_TASK2_FAULT_LINE_LOST, input->now_ms);
            return;
        }
    } else {
        state->line_lost_timer_active = false;
    }

    if (input->grayscale_sequence == state->last_grayscale_sequence) { return; }
    state->last_grayscale_sequence = input->grayscale_sequence;
    stop_mark = (input->black_mask & APP_H723_TASK2_STOP_BLACK_MASK) ==
                APP_H723_TASK2_STOP_BLACK_MASK;

    if (state->phase == APP_TASK2_PHASE_DEPART) {
        if (stop_mark) {
            state->clear_timer_active = false;
        } else if (!state->clear_timer_active) {
            state->clear_timer_active = true;
            state->clear_since_ms = input->now_ms;
        } else if ((input->now_ms - state->clear_since_ms) >=
                   APP_H723_TASK2_DEPART_CLEAR_MS) {
            state->phase = APP_TASK2_PHASE_CRUISE;
        }
    }
    if ((state->phase == APP_TASK2_PHASE_CRUISE) &&
        (state->distance_mm >= APP_H723_TASK2_REARM_DISTANCE_MM)) {
        state->phase = APP_TASK2_PHASE_APPROACH;
    }
    if (state->phase == APP_TASK2_PHASE_APPROACH) {
        if (!stop_mark) {
            state->stop_timer_active = false;
        } else if (!state->stop_timer_active) {
            state->stop_timer_active = true;
            state->stop_since_ms = input->now_ms;
        } else if ((input->now_ms - state->stop_since_ms) >=
                   APP_H723_TASK2_STOP_CONFIRM_MS) {
            state->phase = APP_TASK2_PHASE_STOPPED;
            state->finish_ms = input->now_ms;
        }
    }
}

void app_task2_get_output(const app_task2_state_t *state, app_task2_output_t *output)
{
    uint32_t end_ms;
    if ((state == NULL) || (output == NULL)) { return; }
    (void)memset(output, 0, sizeof(*output));
    output->phase = state->phase;
    output->fault = state->fault;
    output->running = is_running(state->phase);
    output->follow_line = output->running;
    output->stop = (state->phase == APP_TASK2_PHASE_STOPPED) ||
                   (state->phase == APP_TASK2_PHASE_FAULT);
    output->base_speed_mm_s = (state->phase == APP_TASK2_PHASE_APPROACH) ?
        APP_H723_TASK2_APPROACH_SPEED_MM_S :
        (output->running ? APP_H723_TASK2_CRUISE_SPEED_MM_S : 0.0f);
    output->distance_mm = state->distance_mm;
    end_ms = output->running ? state->last_step_ms : state->finish_ms;
    output->elapsed_ms = (state->phase == APP_TASK2_PHASE_IDLE) ? 0U :
                         (end_ms - state->start_ms);
}
