#ifndef APP_TASK2_H
#define APP_TASK2_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_TASK2_PHASE_IDLE = 0,
    APP_TASK2_PHASE_DEPART,
    APP_TASK2_PHASE_CRUISE,
    APP_TASK2_PHASE_APPROACH,
    APP_TASK2_PHASE_STOPPED,
    APP_TASK2_PHASE_FAULT
} app_task2_phase_t;

typedef enum {
    APP_TASK2_FAULT_NONE = 0,
    APP_TASK2_FAULT_ADC_TIMEOUT,
    APP_TASK2_FAULT_LINE_LOST,
    APP_TASK2_FAULT_FEEDBACK_STALE,
    APP_TASK2_FAULT_RUN_TIMEOUT
} app_task2_fault_t;

typedef struct {
    uint32_t now_ms;
    uint32_t grayscale_sequence;
    uint32_t line_strength;
    uint8_t black_mask;
    uint8_t adc_timeout_mask;
    bool left_feedback_fresh;
    bool right_feedback_fresh;
    float left_speed_mm_s;
    float right_speed_mm_s;
} app_task2_input_t;

typedef struct {
    app_task2_phase_t phase;
    app_task2_fault_t fault;
    uint32_t start_ms;
    uint32_t finish_ms;
    uint32_t last_step_ms;
    uint32_t clear_since_ms;
    uint32_t stop_since_ms;
    uint32_t line_lost_since_ms;
    uint32_t last_grayscale_sequence;
    float distance_mm;
    bool clear_timer_active;
    bool stop_timer_active;
    bool line_lost_timer_active;
} app_task2_state_t;

typedef struct {
    app_task2_phase_t phase;
    app_task2_fault_t fault;
    bool running;
    bool follow_line;
    bool stop;
    float base_speed_mm_s;
    float distance_mm;
    uint32_t elapsed_ms;
} app_task2_output_t;

void app_task2_init(app_task2_state_t *state);
void app_task2_start(app_task2_state_t *state, uint32_t now_ms);
void app_task2_abort(app_task2_state_t *state);
void app_task2_step(app_task2_state_t *state, const app_task2_input_t *input);
void app_task2_get_output(const app_task2_state_t *state, app_task2_output_t *output);

#endif
