#include "app_task3.h"

#include <math.h>
#include <string.h>

static bool app_task3_config_is_valid(const app_task3_config_t *config)
{
    return config != NULL &&
           isfinite(config->start_position_deg) &&
           isfinite(config->first_target_position_deg) &&
           isfinite(config->final_target_position_deg) &&
           isfinite(config->wait_time_s) &&
           isfinite(config->finish_display_time_s) &&
           config->wait_time_s > 0.0f &&
           config->finish_display_time_s >= config->wait_time_s;
}

static uint32_t app_task3_seconds_to_ms(float seconds)
{
    return (uint32_t)(seconds * 1000.0f + 0.5f);
}

static void app_task3_publish(const app_task3_t *task,
                              app_task3_output_t *output,
                              uint32_t now_ms)
{
    (void)memset(output, 0, sizeof(*output));
    output->phase = task->phase;
    output->running = task->phase != APP_TASK3_PHASE_IDLE &&
                      task->phase != APP_TASK3_PHASE_FAULT;
    output->config_valid = app_task3_config_is_valid(&task->config);
    if (task->phase == APP_TASK3_PHASE_WAIT_CONFIRM) {
        output->target_position_deg = task->config.start_position_deg;
    } else if (task->phase == APP_TASK3_PHASE_WAIT_FIRST_TARGET) {
        output->target_position_deg = task->config.first_target_position_deg;
        output->elapsed_ms = now_ms - task->first_target_ms;
    } else if (task->phase == APP_TASK3_PHASE_WAIT_FINISH_DISPLAY ||
               task->phase == APP_TASK3_PHASE_FINISHED_WAIT_KEY) {
        output->target_position_deg = task->config.final_target_position_deg;
        output->elapsed_ms = now_ms - task->first_target_ms;
    }
}

void app_task3_init(app_task3_t *task)
{
    if (task == NULL) {
        return;
    }
    (void)memset(task, 0, sizeof(*task));
    task->phase = APP_TASK3_PHASE_IDLE;
}

void app_task3_start(app_task3_t *task,
                     const app_task3_config_t *config,
                     uint32_t now_ms)
{
    if (task == NULL) {
        return;
    }
    if (!app_task3_config_is_valid(config)) {
        task->phase = APP_TASK3_PHASE_FAULT;
        return;
    }
    task->config = *config;
    task->start_ms = now_ms;
    task->first_target_ms = 0U;
    task->phase = APP_TASK3_PHASE_WAIT_CONFIRM;
}

void app_task3_step(app_task3_t *task,
                    const app_task3_input_t *input,
                    app_task3_output_t *output)
{
    uint32_t elapsed_ms;

    if (output == NULL) {
        return;
    }
    if (task == NULL || input == NULL) {
        (void)memset(output, 0, sizeof(*output));
        output->phase = APP_TASK3_PHASE_FAULT;
        return;
    }
    if (task->phase == APP_TASK3_PHASE_WAIT_CONFIRM && input->confirm_pressed) {
        task->first_target_ms = input->now_ms;
        task->phase = APP_TASK3_PHASE_WAIT_FIRST_TARGET;
    } else if (task->phase == APP_TASK3_PHASE_WAIT_FIRST_TARGET) {
        elapsed_ms = input->now_ms - task->first_target_ms;
        if (elapsed_ms >= app_task3_seconds_to_ms(task->config.wait_time_s)) {
            task->phase = APP_TASK3_PHASE_WAIT_FINISH_DISPLAY;
        }
    } else if (task->phase == APP_TASK3_PHASE_WAIT_FINISH_DISPLAY) {
        elapsed_ms = input->now_ms - task->first_target_ms;
        if (elapsed_ms >= app_task3_seconds_to_ms(task->config.finish_display_time_s)) {
            task->phase = APP_TASK3_PHASE_FINISHED_WAIT_KEY;
        }
    } else if (task->phase == APP_TASK3_PHASE_FINISHED_WAIT_KEY &&
               input->confirm_pressed) {
        task->phase = APP_TASK3_PHASE_IDLE;
    }
    app_task3_publish(task, output, input->now_ms);
}

void app_task3_abort(app_task3_t *task)
{
    if (task != NULL) {
        task->phase = APP_TASK3_PHASE_IDLE;
    }
}
