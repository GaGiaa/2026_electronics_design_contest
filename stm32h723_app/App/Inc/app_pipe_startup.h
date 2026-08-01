#ifndef APP_PIPE_STARTUP_H
#define APP_PIPE_STARTUP_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_PIPE_STARTUP_STATE_WAIT_HOME = 0U,
    APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION = 1U,
    APP_PIPE_STARTUP_STATE_READY = 2U,
    APP_PIPE_STARTUP_STATE_ID3_LOCKED = 3U
} app_pipe_startup_state_t;

typedef enum {
    APP_PIPE_STARTUP_FAULT_NONE = 0U,
    APP_PIPE_STARTUP_FAULT_MOVE_FEEDBACK_LOST = 1U,
    APP_PIPE_STARTUP_FAULT_MOVE_ZERO_LOST = 2U,
    APP_PIPE_STARTUP_FAULT_MOVE_TIMEOUT = 3U
} app_pipe_startup_fault_t;

typedef struct {
    bool balance_zero_valid;
    bool id3_feedback_valid;
    float id3_position_deg;
    float id3_output_speed_rpm;
    uint32_t now_ms;
} app_pipe_startup_input_t;

typedef struct {
    app_pipe_startup_state_t state;
    app_pipe_startup_fault_t fault;
    bool id3_allowed;
    bool other_motors_allowed;
    float startup_target_position_deg;
    float id3_position_deg;
    float id3_output_speed_rpm;
    bool startup_position_stable;
} app_pipe_startup_snapshot_t;

typedef struct {
    app_pipe_startup_state_t state;
    app_pipe_startup_fault_t fault;
    uint32_t move_started_ms;
    uint32_t position_stable_started_ms;
    bool startup_position_stable;
} app_pipe_startup_t;

void app_pipe_startup_init(app_pipe_startup_t *startup);
void app_pipe_startup_step(app_pipe_startup_t *startup,
                           const app_pipe_startup_input_t *input,
                           app_pipe_startup_snapshot_t *snapshot);

#endif /* APP_PIPE_STARTUP_H */
