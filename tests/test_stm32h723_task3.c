#include <assert.h>

#include "app_task3.h"

static app_task3_config_t default_config(void)
{
    return (app_task3_config_t){
        .start_position_deg = 143.11f,
        .first_target_position_deg = 128.84f,
        .final_target_position_deg = 150.99f,
        .wait_time_s = 3.0f,
        .finish_display_time_s = 5.0f,
    };
}

static void test_start_waits_for_confirmation_at_start_position(void)
{
    app_task3_t task;
    app_task3_output_t output;
    const app_task3_config_t config = default_config();

    app_task3_init(&task);
    app_task3_start(&task, &config, 100U);
    app_task3_step(&task, &(app_task3_input_t){.now_ms = 100U}, &output);

    assert(output.phase == APP_TASK3_PHASE_WAIT_CONFIRM);
    assert(output.running);
    assert(output.target_position_deg == 143.11f);
    assert(output.elapsed_ms == 0U);
}

static void test_starting_confirmation_is_not_reused(void)
{
    app_task3_t task;
    app_task3_output_t output;
    const app_task3_config_t config = default_config();

    app_task3_init(&task);
    app_task3_start(&task, &config, 100U);
    app_task3_step(&task, &(app_task3_input_t){
        .now_ms = 100U,
        .confirm_pressed = false,
    }, &output);
    assert(output.phase == APP_TASK3_PHASE_WAIT_CONFIRM);
}

static void test_confirm_sets_first_target_and_starts_cumulative_timer(void)
{
    app_task3_t task;
    app_task3_output_t output;
    const app_task3_config_t config = default_config();

    app_task3_init(&task);
    app_task3_start(&task, &config, 100U);
    app_task3_step(&task, &(app_task3_input_t){
        .now_ms = 250U,
        .confirm_pressed = true,
    }, &output);

    assert(output.phase == APP_TASK3_PHASE_WAIT_FIRST_TARGET);
    assert(output.target_position_deg == 128.84f);
    assert(output.elapsed_ms == 0U);

    app_task3_step(&task, &(app_task3_input_t){.now_ms = 1250U}, &output);
    assert(output.elapsed_ms == 1000U);
    assert(output.target_position_deg == 128.84f);
}

static void test_wait_time_switches_to_final_without_resetting_timer(void)
{
    app_task3_t task;
    app_task3_output_t output;
    const app_task3_config_t config = default_config();

    app_task3_init(&task);
    app_task3_start(&task, &config, 0U);
    app_task3_step(&task, &(app_task3_input_t){
        .now_ms = 1U,
        .confirm_pressed = true,
    }, &output);
    app_task3_step(&task, &(app_task3_input_t){.now_ms = 3001U}, &output);

    assert(output.phase == APP_TASK3_PHASE_WAIT_FINISH_DISPLAY);
    assert(output.target_position_deg == 150.99f);
    assert(output.elapsed_ms == 3000U);
}

static void test_finish_display_waits_for_b1_then_returns_idle(void)
{
    app_task3_t task;
    app_task3_output_t output;
    const app_task3_config_t config = default_config();

    app_task3_init(&task);
    app_task3_start(&task, &config, 0U);
    app_task3_step(&task, &(app_task3_input_t){
        .now_ms = 1U,
        .confirm_pressed = true,
    }, &output);
    app_task3_step(&task, &(app_task3_input_t){.now_ms = 3001U}, &output);
    app_task3_step(&task, &(app_task3_input_t){.now_ms = 5001U}, &output);
    assert(output.phase == APP_TASK3_PHASE_FINISHED_WAIT_KEY);
    assert(output.elapsed_ms == 5000U);

    app_task3_step(&task, &(app_task3_input_t){
        .now_ms = 5200U,
        .confirm_pressed = true,
    }, &output);
    assert(output.phase == APP_TASK3_PHASE_IDLE);
    assert(!output.running);
}

static void test_invalid_config_finishes_without_running(void)
{
    app_task3_t task;
    app_task3_output_t output;
    app_task3_config_t config = default_config();

    config.finish_display_time_s = 2.0f;
    app_task3_init(&task);
    app_task3_start(&task, &config, 0U);
    app_task3_step(&task, &(app_task3_input_t){.now_ms = 0U}, &output);

    assert(output.phase == APP_TASK3_PHASE_FAULT);
    assert(!output.running);
}

int main(void)
{
    test_start_waits_for_confirmation_at_start_position();
    test_starting_confirmation_is_not_reused();
    test_confirm_sets_first_target_and_starts_cumulative_timer();
    test_wait_time_switches_to_final_without_resetting_timer();
    test_finish_display_waits_for_b1_then_returns_idle();
    test_invalid_config_finishes_without_running();
    return 0;
}
