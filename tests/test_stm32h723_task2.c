#include <assert.h>
#include <math.h>

#include "app_config.h"
#include "app_task2.h"

static app_task2_input_t input_at(uint32_t now_ms)
{
    app_task2_input_t input = {0};
    input.now_ms = now_ms;
    input.grayscale_sequence = now_ms + 1U;
    input.line_strength = APP_H723_LINE_FOLLOW_LINE_STRENGTH_MIN;
    input.left_feedback_fresh = true;
    input.right_feedback_fresh = true;
    input.left_speed_mm_s = APP_H723_TASK2_CRUISE_SPEED_MM_S;
    input.right_speed_mm_s = APP_H723_TASK2_CRUISE_SPEED_MM_S;
    return input;
}

static void depart(app_task2_state_t *state, uint32_t start_ms)
{
    app_task2_input_t input = input_at(start_ms);
    app_task2_start(state, start_ms);
    input.black_mask = APP_H723_TASK2_STOP_BLACK_MASK;
    app_task2_step(state, &input);
    assert(state->phase == APP_TASK2_PHASE_DEPART);
    input.now_ms += 1U;
    input.grayscale_sequence++;
    input.black_mask = 0U;
    app_task2_step(state, &input);
    input.now_ms += APP_H723_TASK2_DEPART_CLEAR_MS;
    input.grayscale_sequence++;
    app_task2_step(state, &input);
    assert(state->phase == APP_TASK2_PHASE_CRUISE);
}

static void test_start_line_is_ignored_until_departed(void)
{
    app_task2_state_t state;
    app_task2_output_t output;
    app_task2_input_t input = input_at(100U);
    app_task2_init(&state);
    app_task2_start(&state, 100U);
    input.black_mask = APP_H723_TASK2_STOP_BLACK_MASK;
    app_task2_step(&state, &input);
    app_task2_get_output(&state, &output);
    assert(state.phase == APP_TASK2_PHASE_DEPART);
    assert(output.follow_line);
    assert(output.base_speed_mm_s == APP_H723_TASK2_CRUISE_SPEED_MM_S);
}

static void test_distance_gate_selects_approach_speed(void)
{
    app_task2_state_t state;
    app_task2_output_t output;
    app_task2_input_t input;
    app_task2_init(&state);
    depart(&state, 0U);
    state.distance_mm = APP_H723_TASK2_REARM_DISTANCE_MM - 1.0f;
    input = input_at(100U);
    input.left_speed_mm_s = 0.0f;
    input.right_speed_mm_s = 0.0f;
    input.black_mask = APP_H723_TASK2_STOP_BLACK_MASK;
    app_task2_step(&state, &input);
    assert(state.phase == APP_TASK2_PHASE_CRUISE);
    state.distance_mm = APP_H723_TASK2_REARM_DISTANCE_MM;
    input.now_ms++;
    input.grayscale_sequence++;
    app_task2_step(&state, &input);
    app_task2_get_output(&state, &output);
    assert(state.phase == APP_TASK2_PHASE_APPROACH);
    assert(output.base_speed_mm_s == APP_H723_TASK2_APPROACH_SPEED_MM_S);
}

static void test_stop_line_must_remain_stable(void)
{
    app_task2_state_t state;
    app_task2_output_t output;
    app_task2_input_t input;
    app_task2_init(&state);
    depart(&state, 0U);
    state.phase = APP_TASK2_PHASE_APPROACH;
    input = input_at(1000U);
    input.black_mask = APP_H723_TASK2_STOP_BLACK_MASK;
    app_task2_step(&state, &input);
    input.now_ms += APP_H723_TASK2_STOP_CONFIRM_MS - 1U;
    input.grayscale_sequence++;
    app_task2_step(&state, &input);
    assert(state.phase == APP_TASK2_PHASE_APPROACH);
    input.now_ms++;
    input.grayscale_sequence++;
    app_task2_step(&state, &input);
    app_task2_get_output(&state, &output);
    assert(state.phase == APP_TASK2_PHASE_STOPPED);
    assert(output.stop);
    assert(output.elapsed_ms == input.now_ms);
}

static void test_faults_latch_and_abort_returns_idle(void)
{
    app_task2_state_t state;
    app_task2_input_t input;
    app_task2_init(&state);
    depart(&state, 0U);
    input = input_at(100U);
    input.adc_timeout_mask = 1U;
    app_task2_step(&state, &input);
    assert(state.phase == APP_TASK2_PHASE_FAULT);
    assert(state.fault == APP_TASK2_FAULT_ADC_TIMEOUT);
    app_task2_abort(&state);
    assert(state.phase == APP_TASK2_PHASE_IDLE);
}

static void test_line_loss_and_run_timeout_fault(void)
{
    app_task2_state_t state;
    app_task2_input_t input;
    app_task2_init(&state);
    depart(&state, 0U);
    input = input_at(100U);
    input.line_strength = 0U;
    app_task2_step(&state, &input);
    input.now_ms += APP_H723_TASK2_LINE_LOST_TIMEOUT_MS;
    input.grayscale_sequence++;
    app_task2_step(&state, &input);
    assert(state.fault == APP_TASK2_FAULT_LINE_LOST);

    app_task2_start(&state, 1000U);
    input = input_at(1000U + APP_H723_TASK2_RUN_TIMEOUT_MS);
    app_task2_step(&state, &input);
    assert(state.fault == APP_TASK2_FAULT_RUN_TIMEOUT);
}

int main(void)
{
    test_start_line_is_ignored_until_departed();
    test_distance_gate_selects_approach_speed();
    test_stop_line_must_remain_stable();
    test_faults_latch_and_abort_returns_idle();
    test_line_loss_and_run_timeout_fault();
    return 0;
}
