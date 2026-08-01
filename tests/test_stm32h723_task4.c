#include <assert.h>
#include <math.h>

#include "app_config.h"
#include "app_task4.h"

static void assert_near(float actual, float expected, float tolerance)
{
    assert(fabsf(actual - expected) <= tolerance);
}

static app_task4_input_t input_at(uint32_t now_ms, float distance_mm)
{
    app_task4_input_t input = {0};

    input.now_ms = now_ms;
    input.distance_mm = distance_mm;
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
    assert_near(output.base_speed_mm_s, 0.0f, 0.001f);
    assert(output.elapsed_ms == 0U);
    assert_near(output.distance_mm, 0.0f, 0.001f);
}

static void test_time_based_acceleration_ramps_up(void)
{
    /* 时间加速：t=500ms, speed = 100*0.5 = 50 mm/s。距离无关。 */
    app_task4_state_t state;
    app_task4_output_t output;
    app_task4_input_t in;

    app_task4_init(&state);
    app_task4_start(&state, 100U, 0.0f);

    in = input_at(600U, 0.0f);
    app_task4_step(&state, &in);
    app_task4_get_output(&state, &output);
    assert(output.phase == APP_TASK4_PHASE_RUNNING);
    assert(output.follow_line);
    assert_near(output.base_speed_mm_s, 50.0f, 0.2f);
}

static void test_reaches_cruise_speed_by_time(void)
{
    /* t=3600ms, speed 应已攀升到当前巡航速度 300 mm/s。 */
    app_task4_state_t state;
    app_task4_output_t output;
    app_task4_input_t in;

    app_task4_init(&state);
    app_task4_start(&state, 0U, 0.0f);

    in = input_at(3600U, 0.0f);
    app_task4_step(&state, &in);
    app_task4_get_output(&state, &output);
    assert(output.phase == APP_TASK4_PHASE_RUNNING);
    assert_near(output.base_speed_mm_s, APP_H723_TASK4_CRUISE_SPEED_MM_S, 0.2f);
}

static void test_distance_braking_overrides_time(void)
{
    /* 当前巡航速度为 300 mm/s，制动距离为 450 mm；行驶到 1100 mm
       后剩余 400 mm，应由距离制动覆盖时间加速结果。 */
    app_task4_state_t state;
    app_task4_output_t output;
    app_task4_input_t in;

    app_task4_init(&state);
    app_task4_start(&state, 0U, 0.0f);

    in = input_at(5000U, 1100.0f);
    app_task4_step(&state, &in);
    app_task4_get_output(&state, &output);
    assert(output.phase == APP_TASK4_PHASE_RUNNING);
    assert(output.base_speed_mm_s < APP_H723_TASK4_CRUISE_SPEED_MM_S);
    assert(output.base_speed_mm_s > 0.0f);
}

static void test_stops_at_target_distance(void)
{
    app_task4_state_t state;
    app_task4_output_t output;
    app_task4_input_t in;

    app_task4_init(&state);
    app_task4_start(&state, 0U, 0.0f);

    in = input_at(5000U, APP_H723_TASK4_A_TO_B_DISTANCE_MM);
    app_task4_step(&state, &in);
    app_task4_get_output(&state, &output);
    assert(output.phase == APP_TASK4_PHASE_STOPPED);
    assert(!output.running);
    assert(!output.follow_line);
    assert_near(output.base_speed_mm_s, 0.0f, 0.001f);
}

static void test_timeout_fallback_stops(void)
{
    app_task4_state_t state;
    app_task4_output_t output;
    app_task4_input_t in;

    app_task4_init(&state);
    app_task4_start(&state, 0U, 0.0f);

    in = input_at(APP_H723_TASK4_RUN_TIMEOUT_MS, 500.0f);
    app_task4_step(&state, &in);
    app_task4_get_output(&state, &output);
    assert(output.phase == APP_TASK4_PHASE_STOPPED);
    assert(output.elapsed_ms == APP_H723_TASK4_RUN_TIMEOUT_MS);
}

static void test_abort_returns_to_idle(void)
{
    app_task4_state_t state;

    app_task4_init(&state);
    app_task4_start(&state, 100U, 0.0f);
    app_task4_abort(&state);

    assert(state.phase == APP_TASK4_PHASE_IDLE);
}

static void test_stopped_state_outputs_stop_flag(void)
{
    app_task4_state_t state;
    app_task4_output_t output;
    app_task4_input_t in;

    app_task4_init(&state);
    app_task4_start(&state, 0U, 0.0f);

    in = input_at(4000U, APP_H723_TASK4_A_TO_B_DISTANCE_MM);
    app_task4_step(&state, &in);
    app_task4_get_output(&state, &output);

    assert(output.stop);
    assert(!output.follow_line);
    assert_near(output.distance_mm, APP_H723_TASK4_A_TO_B_DISTANCE_MM, 0.1f);
}

static void test_distance_field_flows_through_output(void)
{
    app_task4_state_t state;
    app_task4_output_t output;
    app_task4_input_t in;

    app_task4_init(&state);
    app_task4_start(&state, 0U, 200.0f);

    in = input_at(3000U, 1000.0f);
    app_task4_step(&state, &in);
    app_task4_get_output(&state, &output);
    assert(output.phase == APP_TASK4_PHASE_RUNNING);
    assert_near(output.distance_mm, 800.0f, 0.1f);
}

int main(void)
{
    test_initial_state_is_idle();
    test_time_based_acceleration_ramps_up();
    test_reaches_cruise_speed_by_time();
    test_distance_braking_overrides_time();
    test_stops_at_target_distance();
    test_timeout_fallback_stops();
    test_abort_returns_to_idle();
    test_stopped_state_outputs_stop_flag();
    test_distance_field_flows_through_output();
    return 0;
}
