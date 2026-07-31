#ifndef APP_TASK4_H
#define APP_TASK4_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_TASK4_PHASE_IDLE = 0,
    APP_TASK4_PHASE_RUNNING,
    APP_TASK4_PHASE_STOPPED
} app_task4_phase_t;

typedef struct {
    uint32_t now_ms;
} app_task4_input_t;

typedef struct {
    app_task4_phase_t phase;
    uint32_t start_ms;
    uint32_t finish_ms;
    uint32_t last_step_ms;
} app_task4_state_t;

typedef struct {
    app_task4_phase_t phase;
    bool running;
    bool follow_line;
    bool stop;
    float base_speed_mm_s;
    uint32_t elapsed_ms;
} app_task4_output_t;

void app_task4_init(app_task4_state_t *state);
void app_task4_start(app_task4_state_t *state, uint32_t now_ms);
void app_task4_abort(app_task4_state_t *state);
void app_task4_step(app_task4_state_t *state, const app_task4_input_t *input);
void app_task4_get_output(const app_task4_state_t *state, app_task4_output_t *output);

#endif
