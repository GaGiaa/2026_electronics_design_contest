#ifndef APP_PIPE_STARTUP_H
#define APP_PIPE_STARTUP_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_PIPE_STARTUP_STATE_WAIT_HOME = 0U,
    APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED = 1U,
    APP_PIPE_STARTUP_STATE_READY = 2U,
    APP_PIPE_STARTUP_STATE_ID3_LOCKED = 3U
} app_pipe_startup_state_t;

/* Button bits are supplied by the board input layer. PA6 is intentionally ignored. */
#define APP_PIPE_STARTUP_BUTTON_CAPTURE (1U << 0U) /* PC5 */
#define APP_PIPE_STARTUP_BUTTON_SKIP (1U << 1U)    /* PC4 */
#define APP_PIPE_STARTUP_BUTTON_PA6 (1U << 2U)     /* legacy/unrelated input */

typedef struct {
    bool balance_zero_valid;
    bool imu_pitch_valid;
    bool imu_pitch_fresh;
    float imu_pitch_deg;
    uint32_t buttons;
} app_pipe_startup_input_t;

typedef struct {
    app_pipe_startup_state_t state;
    bool calibration_valid;
    float captured_pitch_deg;
    bool id3_allowed;
    bool other_motors_allowed;
} app_pipe_startup_snapshot_t;

typedef struct {
    app_pipe_startup_state_t state;
    bool calibration_valid;
    float captured_pitch_deg;
    uint32_t previous_buttons;
} app_pipe_startup_t;

void app_pipe_startup_init(app_pipe_startup_t *startup);
void app_pipe_startup_step(app_pipe_startup_t *startup,
                           const app_pipe_startup_input_t *input,
                           app_pipe_startup_snapshot_t *snapshot);

#endif /* APP_PIPE_STARTUP_H */
