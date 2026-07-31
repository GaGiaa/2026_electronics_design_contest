#ifndef APP_TASK56_H
#define APP_TASK56_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_TASK56_PHASE_IDLE = 0,
    APP_TASK56_PHASE_RUNNING,
    APP_TASK56_PHASE_STOPPED
} app_task56_phase_t;

typedef struct {
    uint32_t now_ms;
} app_task56_input_t;

typedef struct {
    app_task56_phase_t phase;
    uint32_t start_ms;
    uint32_t finish_ms;
    uint32_t last_step_ms;
} app_task56_state_t;

typedef struct {
    app_task56_phase_t phase;
    bool running;
    bool follow_line;
    bool stop;
    float base_speed_mm_s;
    uint32_t elapsed_ms;
} app_task56_output_t;

void app_task56_init(app_task56_state_t *state);
void app_task56_start(app_task56_state_t *state, uint32_t now_ms);
void app_task56_abort(app_task56_state_t *state);
void app_task56_step(app_task56_state_t *state,
                     const app_task56_input_t *input);
void app_task56_get_output(const app_task56_state_t *state,
                           app_task56_output_t *output);

#endif
