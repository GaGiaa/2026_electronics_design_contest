#include <assert.h>

#include "app_config.h"
#include "app_task2.h"

static app_task2_input_t input_at(uint32_t now_ms, uint8_t black_count)
{
    app_task2_input_t input = {0};

    input.now_ms = now_ms;
    input.black_count = black_count;
    return input;
}

static void test_initial_state_is_idle(void)
{
    app_task2_state_t state;
    app_task2_output_t output;

    app_task2_init(&state);
    app_task2_get_output(&state, &output);

    assert(output.phase == APP_TASK2_PHASE_IDLE);
    assert(!output.running);
    assert(!output.follow_line);
    assert(!output.stop);
    assert(output.base_speed_mm_s == 0.0f);
}

static void test_start_enters_fixed_speed_line_follow(void)
{
    app_task2_state_t state;
    app_task2_output_t output;
    app_task2_input_t input = input_at(100U, 0U);

    app_task2_init(&state);
    app_task2_start(&state, 100U);
    app_task2_step(&state, &input);
    app_task2_get_output(&state, &output);

    assert(output.phase == APP_TASK2_PHASE_RUNNING);
    assert(output.running);
    assert(output.follow_line);
    assert(!output.stop);
    assert(output.base_speed_mm_s == APP_H723_TASK2_SPEED_MM_S);
}

static void test_black_count_below_stop_threshold_keeps_running(void)
{
    app_task2_state_t state;
    app_task2_output_t output;
    app_task2_input_t input = input_at(100U, APP_H723_TASK2_STOP_BLACK_COUNT - 1U);

    app_task2_init(&state);
    app_task2_start(&state, 100U);
    app_task2_step(&state, &input);
    app_task2_get_output(&state, &output);

    assert(state.phase == APP_TASK2_PHASE_RUNNING);
    assert(output.follow_line);
    assert(output.base_speed_mm_s == APP_H723_TASK2_SPEED_MM_S);
}

static void test_stop_line_immediately_latches_zero_speed(void)
{
    app_task2_state_t state;
    app_task2_output_t output;
    app_task2_input_t input = input_at(100U, APP_H723_TASK2_STOP_BLACK_COUNT);

    app_task2_init(&state);
    app_task2_start(&state, 100U);
    app_task2_step(&state, &input);
    app_task2_get_output(&state, &output);

    assert(output.phase == APP_TASK2_PHASE_STOPPED);
    assert(!output.running);
    assert(!output.follow_line);
    assert(output.stop);
    assert(output.base_speed_mm_s == 0.0f);
    assert(output.elapsed_ms == 0U);
}

static void test_abort_returns_to_idle(void)
{
    app_task2_state_t state;

    app_task2_init(&state);
    app_task2_start(&state, 100U);
    app_task2_abort(&state);

    assert(state.phase == APP_TASK2_PHASE_IDLE);
}

int main(void)
{
    test_initial_state_is_idle();
    test_start_enters_fixed_speed_line_follow();
    test_black_count_below_stop_threshold_keeps_running();
    test_stop_line_immediately_latches_zero_speed();
    test_abort_returns_to_idle();
    return 0;
}
