#ifndef APP_TASK2_H
#define APP_TASK2_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_TASK2_PHASE_IDLE = 0,
    APP_TASK2_PHASE_RUNNING,
    APP_TASK2_PHASE_STOPPED
} app_task2_phase_t;

typedef struct {
    uint32_t now_ms;
    uint8_t black_count;
} app_task2_input_t;

typedef struct {
    app_task2_phase_t phase;
    uint32_t start_ms;
    uint32_t finish_ms;
    uint32_t last_step_ms;
} app_task2_state_t;

typedef struct {
    app_task2_phase_t phase;
    bool running;
    bool follow_line;
    bool stop;
    float base_speed_mm_s;
    uint32_t elapsed_ms;
} app_task2_output_t;

void app_task2_init(app_task2_state_t *state);
void app_task2_start(app_task2_state_t *state, uint32_t now_ms);
void app_task2_abort(app_task2_state_t *state);
void app_task2_step(app_task2_state_t *state, const app_task2_input_t *input);
void app_task2_get_output(const app_task2_state_t *state, app_task2_output_t *output);

#endif
