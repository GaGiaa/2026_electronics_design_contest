#include <assert.h>
#include <math.h>

#include "app_config.h"
#include "app_task56.h"

static void assert_near(float actual, float expected, float tolerance)
{
    assert(fabsf(actual - expected) <= tolerance);
}

static void test_initial_state_is_idle(void)
{
    app_task56_state_t state;
    app_task56_output_t output;

    app_task56_init(&state);
    app_task56_get_output(&state, &output);

    assert(output.phase == APP_TASK56_PHASE_IDLE);
    assert(!output.running);
    assert(!output.follow_line);
    assert_near(output.base_speed_mm_s, 0.0f, 0.001f);
    assert(output.elapsed_ms == 0U);
}

static void test_start_uses_smooth_speed_profile(void)
{
    app_task56_state_t state;
    app_task56_output_t output;

    app_task56_init(&state);
    app_task56_start(&state, 100U);

    app_task56_step(&state, &(app_task56_input_t){.now_ms = 100U});
    app_task56_get_output(&state, &output);
    assert(output.phase == APP_TASK56_PHASE_RUNNING);
    assert(output.follow_line);
    assert_near(output.base_speed_mm_s, 0.0f, 0.001f);

    app_task56_step(&state, &(app_task56_input_t){.now_ms = 467U});
    app_task56_get_output(&state, &output);
    assert_near(output.base_speed_mm_s, 36.7f, 0.2f);

    app_task56_step(&state, &(app_task56_input_t){.now_ms = 2300U});
    app_task56_get_output(&state, &output);
    assert_near(output.base_speed_mm_s,
                APP_H723_TASK56_CRUISE_SPEED_MM_S, 0.2f);
}

static void test_cruise_then_brakes_and_stops_at_timeout(void)
{
    app_task56_state_t state;
    app_task56_output_t output;

    app_task56_init(&state);
    app_task56_start(&state, 0U);

    app_task56_step(&state, &(app_task56_input_t){.now_ms = 29000U});
    app_task56_get_output(&state, &output);
    assert(output.phase == APP_TASK56_PHASE_RUNNING);
    assert_near(output.base_speed_mm_s, 100.0f, 0.2f);

    app_task56_step(&state, &(app_task56_input_t){.now_ms = 29300U});
    app_task56_get_output(&state, &output);
    assert(output.phase == APP_TASK56_PHASE_RUNNING);
    assert(output.base_speed_mm_s > 0.0f);
    assert(output.base_speed_mm_s < APP_H723_TASK56_CRUISE_SPEED_MM_S);

    app_task56_step(&state, &(app_task56_input_t){
        .now_ms = APP_H723_TASK56_RUN_TIMEOUT_MS
    });
    app_task56_get_output(&state, &output);
    assert(output.phase == APP_TASK56_PHASE_STOPPED);
    assert(!output.running);
    assert(!output.follow_line);
    assert_near(output.base_speed_mm_s, 0.0f, 0.001f);
    assert(output.elapsed_ms == APP_H723_TASK56_RUN_TIMEOUT_MS);
}

static void test_stop_state_can_be_started_again(void)
{
    app_task56_state_t state;
    app_task56_output_t output;

    app_task56_init(&state);
    app_task56_start(&state, 0U);
    app_task56_step(&state, &(app_task56_input_t){
        .now_ms = APP_H723_TASK56_RUN_TIMEOUT_MS
    });

    app_task56_start(&state, 50000U);
    app_task56_get_output(&state, &output);
    assert(output.phase == APP_TASK56_PHASE_RUNNING);
    assert(output.running);
    assert(output.elapsed_ms == 0U);
    assert_near(output.base_speed_mm_s, 0.0f, 0.001f);
}

int main(void)
{
    test_initial_state_is_idle();
    test_start_uses_smooth_speed_profile();
    test_cruise_then_brakes_and_stops_at_timeout();
    test_stop_state_can_be_started_again();
    return 0;
}
