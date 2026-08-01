#include "app_task3.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static bool app_task3_config_is_valid(const app_task3_config_t *config)
{
    return config != NULL &&
           isfinite(config->start_target_mm) &&
           isfinite(config->move_target_mm) &&
           isfinite(config->switch_threshold_mm) &&
           isfinite(config->finish_target_mm) &&
           config->start_target_mm >= 0.0f &&
           config->finish_target_mm >= 0.0f &&
           config->move_target_mm > config->switch_threshold_mm &&
           config->switch_threshold_mm > config->start_target_mm;
}

static uint32_t app_task3_elapsed_ms(const app_task3_t *task, uint32_t now_ms)
{
    if (task->phase == APP_TASK3_PHASE_WAIT_START_KEY ||
        task->phase == APP_TASK3_PHASE_IDLE ||
        task->phase == APP_TASK3_PHASE_FAULT) {
        return task->elapsed_ms;
    }
    return now_ms - task->timer_start_ms;
}

static void app_task3_publish(const app_task3_t *task,
                              const app_task3_input_t *input,
                              app_task3_output_t *output)
{
    (void)memset(output, 0, sizeof(*output));
    output->phase = task->phase;
    output->running = task->phase != APP_TASK3_PHASE_IDLE &&
                      task->phase != APP_TASK3_PHASE_FAULT;
    output->config_valid = app_task3_config_is_valid(&task->config);
    output->measured_valid = input->measured_valid && isfinite(input->measured_mm);
    output->measured_mm = input->measured_mm;
    output->elapsed_ms = app_task3_elapsed_ms(task, input->now_ms);
    if (task->phase == APP_TASK3_PHASE_WAIT_START_KEY) {
        output->target_mm = task->config.start_target_mm;
    } else if (task->phase == APP_TASK3_PHASE_MOVE_TO_225) {
        output->target_mm = task->config.move_target_mm;
    } else if (task->phase == APP_TASK3_PHASE_WAIT_FINISH_KEY) {
        output->target_mm = task->config.finish_target_mm;
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
        task->elapsed_ms = 0U;
        return;
    }
    task->config = *config;
    task->timer_start_ms = now_ms;
    task->elapsed_ms = 0U;
    task->phase = APP_TASK3_PHASE_WAIT_START_KEY;
}

void app_task3_step(app_task3_t *task,
                    const app_task3_input_t *input,
                    app_task3_output_t *output)
{
    if (output == NULL) {
        return;
    }
    if (task == NULL || input == NULL) {
        (void)memset(output, 0, sizeof(*output));
        output->phase = APP_TASK3_PHASE_FAULT;
        return;
    }

    if (task->phase == APP_TASK3_PHASE_WAIT_START_KEY &&
        input->confirm_pressed) {
        task->timer_start_ms = input->now_ms;
        task->elapsed_ms = 0U;
        task->phase = APP_TASK3_PHASE_MOVE_TO_225;
    } else if (task->phase == APP_TASK3_PHASE_MOVE_TO_225 &&
               input->measured_valid &&
               isfinite(input->measured_mm) &&
               input->measured_mm >= task->config.switch_threshold_mm) {
        task->elapsed_ms = input->now_ms - task->timer_start_ms;
        task->phase = APP_TASK3_PHASE_WAIT_FINISH_KEY;
    } else if (task->phase == APP_TASK3_PHASE_WAIT_FINISH_KEY &&
               input->confirm_pressed) {
        task->elapsed_ms = input->now_ms - task->timer_start_ms;
        task->phase = APP_TASK3_PHASE_IDLE;
    }
    app_task3_publish(task, input, output);
}

void app_task3_abort(app_task3_t *task)
{
    if (task != NULL) {
        task->phase = APP_TASK3_PHASE_IDLE;
        task->elapsed_ms = 0U;
    }
}
