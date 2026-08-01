#include <assert.h>

#include "app_config.h"
#include "app_task4.h"

static app_task4_input_t input_at(uint32_t now_ms)
{
    app_task4_input_t input = {0};

    input.now_ms = now_ms;
    return input;
}

static void test_initial_state_is_idle(void)
{
    app_task4_state_t state;
    app_task4_output_t output;

    app_task4_init(&state);
    app_task4_get_output(&state, &output);

    assert(output.phase == APP_TASK4_PHASE_IDLE);
    assert(!output.running);
    assert(!output.follow_line);
    assert(output.base_speed_mm_s == 0.0f);
    assert(output.elapsed_ms == 0U);
}

static void test_start_ramps_to_cruise_speed(void)
{
    app_task4_state_t state;
    app_task4_output_t output;

    app_task4_init(&state);
    app_task4_start(&state, 100U);

    app_task4_step(&state, &(app_task4_input_t){.now_ms = 100U});
    app_task4_get_output(&state, &output);
    assert(output.phase == APP_TASK4_PHASE_RUNNING);
    assert(output.follow_line);
    assert(output.base_speed_mm_s == 0.0f);

    app_task4_step(&state, &(app_task4_input_t){.now_ms = 600U});
    app_task4_get_output(&state, &output);
    assert(output.base_speed_mm_s > 49.0f);
    assert(output.base_speed_mm_s < 51.0f);

    app_task4_step(&state, &(app_task4_input_t){.now_ms = 3600U});
    app_task4_get_output(&state, &output);
    assert(output.base_speed_mm_s == APP_H723_TASK4_CRUISE_SPEED_MM_S);
}

static void test_cruise_then_smooth_braking(void)
{
    app_task4_state_t state;
    app_task4_output_t output;

    app_task4_init(&state);
    app_task4_start(&state, 0U);

    app_task4_step(&state, &(app_task4_input_t){.now_ms = 6000U});
    app_task4_get_output(&state, &output);
    assert(output.phase == APP_TASK4_PHASE_RUNNING);
    assert(output.base_speed_mm_s == APP_H723_TASK4_CRUISE_SPEED_MM_S);

    app_task4_step(&state, &(app_task4_input_t){.now_ms = 8000U});
    app_task4_get_output(&state, &output);
    assert(output.phase == APP_TASK4_PHASE_RUNNING);
    assert(output.base_speed_mm_s > 0.0f);
    assert(output.base_speed_mm_s < APP_H723_TASK4_CRUISE_SPEED_MM_S);

    app_task4_step(&state, &(app_task4_input_t){.now_ms = APP_H723_TASK4_RUN_TIMEOUT_MS});
    app_task4_get_output(&state, &output);
    assert(output.phase == APP_TASK4_PHASE_STOPPED);
    assert(!output.running);
    assert(!output.follow_line);
    assert(output.base_speed_mm_s == 0.0f);
    assert(output.elapsed_ms == APP_H723_TASK4_RUN_TIMEOUT_MS);
}

static void test_abort_returns_to_idle(void)
{
    app_task4_state_t state;

    app_task4_init(&state);
    app_task4_start(&state, 100U);
    app_task4_abort(&state);

    assert(state.phase == APP_TASK4_PHASE_IDLE);
}

int main(void)
{
    (void)input_at;
    test_initial_state_is_idle();
    test_start_ramps_to_cruise_speed();
    test_cruise_then_smooth_braking();
    test_abort_returns_to_idle();
    return 0;
}
