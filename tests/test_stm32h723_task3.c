#include <assert.h>

#include "app_task3.h"

static app_task3_config_t default_config(void)
{
    return (app_task3_config_t){
        .start_target_mm = 125.0f,
        .move_target_mm = 225.0f,
        .switch_threshold_mm = 175.0f,
        .finish_target_mm = 75.0f,
    };
}

static void test_start_holds_initial_target_until_b1(void)
{
    app_task3_t task;
    app_task3_output_t output;
    const app_task3_config_t config = default_config();

    app_task3_init(&task);
    app_task3_start(&task, &config, 100U);
    app_task3_step(&task, &(app_task3_input_t){.now_ms = 100U}, &output);

    assert(output.phase == APP_TASK3_PHASE_WAIT_START_KEY);
    assert(output.running);
    assert(output.target_mm == 125.0f);
    assert(output.elapsed_ms == 0U);
}

static void test_first_b1_switches_to_225_and_starts_timer(void)
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

    assert(output.phase == APP_TASK3_PHASE_MOVE_TO_225);
    assert(output.target_mm == 225.0f);
    assert(output.elapsed_ms == 0U);

    app_task3_step(&task, &(app_task3_input_t){.now_ms = 1250U}, &output);
    assert(output.elapsed_ms == 1000U);
    assert(output.target_mm == 225.0f);
}

static void test_invalid_or_below_threshold_position_does_not_switch_target(void)
{
    app_task3_t task;
    app_task3_output_t output;
    const app_task3_config_t config = default_config();

    app_task3_init(&task);
    app_task3_start(&task, &config, 0U);
    app_task3_step(&task, &(app_task3_input_t){
        .now_ms = 10U,
        .confirm_pressed = true,
    }, &output);

    app_task3_step(&task, &(app_task3_input_t){
        .now_ms = 1000U,
        .measured_valid = false,
        .measured_mm = 180.0f,
    }, &output);
    assert(output.phase == APP_TASK3_PHASE_MOVE_TO_225);
    assert(output.target_mm == 225.0f);

    app_task3_step(&task, &(app_task3_input_t){
        .now_ms = 1100U,
        .measured_valid = true,
        .measured_mm = 174.9f,
    }, &output);
    assert(output.phase == APP_TASK3_PHASE_MOVE_TO_225);
    assert(output.target_mm == 225.0f);
}

static void test_threshold_switches_to_75_without_resetting_timer(void)
{
    app_task3_t task;
    app_task3_output_t output;
    const app_task3_config_t config = default_config();

    app_task3_init(&task);
    app_task3_start(&task, &config, 0U);
    app_task3_step(&task, &(app_task3_input_t){
        .now_ms = 10U,
        .confirm_pressed = true,
    }, &output);
    app_task3_step(&task, &(app_task3_input_t){
        .now_ms = 2010U,
        .measured_valid = true,
        .measured_mm = 175.0f,
    }, &output);

    assert(output.phase == APP_TASK3_PHASE_WAIT_FINISH_KEY);
    assert(output.target_mm == 75.0f);
    assert(output.elapsed_ms == 2000U);
}

static void test_final_b1_finishes_and_keeps_final_elapsed_time(void)
{
    app_task3_t task;
    app_task3_output_t output;
    const app_task3_config_t config = default_config();

    app_task3_init(&task);
    app_task3_start(&task, &config, 0U);
    app_task3_step(&task, &(app_task3_input_t){
        .now_ms = 10U,
        .confirm_pressed = true,
    }, &output);
    app_task3_step(&task, &(app_task3_input_t){
        .now_ms = 1010U,
        .measured_valid = true,
        .measured_mm = 175.0f,
    }, &output);
    app_task3_step(&task, &(app_task3_input_t){
        .now_ms = 1510U,
        .confirm_pressed = true,
    }, &output);

    assert(output.phase == APP_TASK3_PHASE_IDLE);
    assert(!output.running);
    assert(output.elapsed_ms == 1500U);
}

static void test_invalid_config_enters_fault(void)
{
    app_task3_t task;
    app_task3_output_t output;
    app_task3_config_t config = default_config();

    config.switch_threshold_mm = 250.0f;
    app_task3_init(&task);
    app_task3_start(&task, &config, 0U);
    app_task3_step(&task, &(app_task3_input_t){.now_ms = 0U}, &output);

    assert(output.phase == APP_TASK3_PHASE_FAULT);
    assert(!output.running);
}

int main(void)
{
    test_start_holds_initial_target_until_b1();
    test_first_b1_switches_to_225_and_starts_timer();
    test_invalid_or_below_threshold_position_does_not_switch_target();
    test_threshold_switches_to_75_without_resetting_timer();
    test_final_b1_finishes_and_keeps_final_elapsed_time();
    test_invalid_config_enters_fault();
    return 0;
}
