#ifndef APP_PIPE_STARTUP_H
#define APP_PIPE_STARTUP_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_PIPE_STARTUP_STATE_WAIT_HOME = 0U,
    APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION = 1U,
    APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED = 2U,
    APP_PIPE_STARTUP_STATE_READY = 3U,
    APP_PIPE_STARTUP_STATE_ID3_LOCKED = 4U
} app_pipe_startup_state_t;

typedef enum {
    APP_PIPE_STARTUP_FAULT_NONE = 0U,
    APP_PIPE_STARTUP_FAULT_MOVE_FEEDBACK_LOST = 1U,
    APP_PIPE_STARTUP_FAULT_MOVE_ZERO_LOST = 2U,
    APP_PIPE_STARTUP_FAULT_MOVE_TIMEOUT = 3U,
    APP_PIPE_STARTUP_FAULT_USER_SKIPPED = 4U
} app_pipe_startup_fault_t;

/* Button bits are supplied by the board input layer. PA6 is intentionally ignored. */
#define APP_PIPE_STARTUP_BUTTON_CAPTURE (1U << 0U) /* PC5 */
#define APP_PIPE_STARTUP_BUTTON_SKIP (1U << 1U)    /* PC4 */
#define APP_PIPE_STARTUP_BUTTON_PA6 (1U << 2U)     /* legacy/unrelated input */

typedef struct {
    bool balance_zero_valid;
    bool id3_feedback_valid;
    float id3_position_deg;
    float id3_output_speed_rpm;
    uint32_t now_ms;
    bool imu_pitch_valid;
    bool imu_pitch_fresh;
    float imu_pitch_deg;
    uint32_t buttons;
} app_pipe_startup_input_t;

typedef struct {
    app_pipe_startup_state_t state;
    app_pipe_startup_fault_t fault;
    bool calibration_valid;
    float captured_pitch_deg;
    bool id3_allowed;
    bool other_motors_allowed;
    float calibration_target_position_deg;
    float id3_position_deg;
    float id3_output_speed_rpm;
    bool calibration_position_stable;
} app_pipe_startup_snapshot_t;

typedef struct {
    app_pipe_startup_state_t state;
    app_pipe_startup_fault_t fault;
    bool calibration_valid;
    float captured_pitch_deg;
    uint32_t previous_buttons;
    uint32_t move_started_ms;
    uint32_t position_stable_started_ms;
    bool calibration_position_stable;
} app_pipe_startup_t;

void app_pipe_startup_init(app_pipe_startup_t *startup);
void app_pipe_startup_step(app_pipe_startup_t *startup,
                           const app_pipe_startup_input_t *input,
                           app_pipe_startup_snapshot_t *snapshot);

#endif /* APP_PIPE_STARTUP_H */
