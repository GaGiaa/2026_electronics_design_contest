#ifndef APP_TASK3_H
#define APP_TASK3_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_TASK3_PHASE_IDLE = 0U,
    APP_TASK3_PHASE_WAIT_START_KEY,
    APP_TASK3_PHASE_MOVE_TO_225,
    APP_TASK3_PHASE_WAIT_FINISH_KEY,
    APP_TASK3_PHASE_FAULT
} app_task3_phase_t;

typedef struct {
    float start_target_mm;
    float move_target_mm;
    float switch_threshold_mm;
    float finish_target_mm;
} app_task3_config_t;

typedef struct {
    uint32_t now_ms;
    bool confirm_pressed;
    bool measured_valid;
    float measured_mm;
} app_task3_input_t;

typedef struct {
    app_task3_phase_t phase;
    bool running;
    bool config_valid;
    bool measured_valid;
    float target_mm;
    float measured_mm;
    uint32_t elapsed_ms;
} app_task3_output_t;

typedef struct {
    app_task3_config_t config;
    app_task3_phase_t phase;
    uint32_t timer_start_ms;
    uint32_t elapsed_ms;
} app_task3_t;

void app_task3_init(app_task3_t *task);
void app_task3_start(app_task3_t *task,
                     const app_task3_config_t *config,
                     uint32_t now_ms);
void app_task3_step(app_task3_t *task,
                    const app_task3_input_t *input,
                    app_task3_output_t *output);
void app_task3_abort(app_task3_t *task);

#endif
