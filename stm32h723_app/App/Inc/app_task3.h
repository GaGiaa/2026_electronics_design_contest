#ifndef APP_TASK3_H
#define APP_TASK3_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_TASK3_PHASE_IDLE = 0U,
    APP_TASK3_PHASE_WAIT_CONFIRM,
    APP_TASK3_PHASE_WAIT_FIRST_TARGET,
    APP_TASK3_PHASE_WAIT_FINISH_DISPLAY,
    APP_TASK3_PHASE_FINISHED_WAIT_KEY,
    APP_TASK3_PHASE_FAULT
} app_task3_phase_t;

typedef struct {
    float start_position_deg;
    float first_target_position_deg;
    float final_target_position_deg;
    float wait_time_s;
    float finish_display_time_s;
} app_task3_config_t;

typedef struct {
    uint32_t now_ms;
    bool confirm_pressed;
} app_task3_input_t;

typedef struct {
    app_task3_phase_t phase;
    bool running;
    float target_position_deg;
    uint32_t elapsed_ms;
    bool config_valid;
} app_task3_output_t;

typedef struct {
    app_task3_config_t config;
    app_task3_phase_t phase;
    uint32_t start_ms;
    uint32_t first_target_ms;
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
