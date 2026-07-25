#include "app/app_tasks_motor.h"

#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#include "algorithms/line_control/line_control.h"
#include "algorithms/motor_control/motor_control.h"
#include "app/app_state.h"
#include "config/app_config.h"
#include "config/crsf_config.h"
#include "drivers/encoder/board_encoder.h"
#include "drivers/motor/board_motor.h"
#if CRSF_REMOTE_CONTROL_ENABLE
#include "protocols/crsf/crsf_control.h"
#endif

static StaticTask_t g_motor_task_buffer;
static StackType_t g_motor_task_stack[APP_MOTOR_TASK_STACK_DEPTH];

static void motor_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(10U);
    board_encoder_sample_t samples[BOARD_MOTOR_COUNT];
#if CRSF_REMOTE_CONTROL_ENABLE
    crsf_control_input_t crsf_input;
    board_grayscale_snapshot_t grayscale;
    line_control_state_t line_control_state;
    line_control_output_t line_output;
    app_drive_control_snapshot_t drive_snapshot;
    float crsf_targets[BOARD_MOTOR_COUNT];
    float base_speed_mm_per_s;
    uint32_t control_sequence = 0U;
    crsf_drive_mode_t last_mode = CRSF_DRIVE_MODE_IDLE;
#endif

    (void)argument;
#if CRSF_REMOTE_CONTROL_ENABLE
    line_control_init(&line_control_state);
#endif
    for (;;) {
        app_state_encoder_cycle_begin();
        samples[BOARD_MOTOR_FRONT_LEFT] =
            board_encoder_sample(BOARD_MOTOR_FRONT_LEFT);
        samples[BOARD_MOTOR_FRONT_RIGHT] =
            board_encoder_sample(BOARD_MOTOR_FRONT_RIGHT);
        samples[BOARD_MOTOR_REAR_LEFT] =
            board_encoder_sample(BOARD_MOTOR_REAR_LEFT);
        samples[BOARD_MOTOR_REAR_RIGHT] =
            board_encoder_sample(BOARD_MOTOR_REAR_RIGHT);
        app_state_encoder_sample_publish(BOARD_MOTOR_FRONT_LEFT,
                                          samples[BOARD_MOTOR_FRONT_LEFT]);
        app_state_encoder_sample_publish(BOARD_MOTOR_FRONT_RIGHT,
                                          samples[BOARD_MOTOR_FRONT_RIGHT]);
        app_state_encoder_sample_publish(BOARD_MOTOR_REAR_LEFT,
                                          samples[BOARD_MOTOR_REAR_LEFT]);
        app_state_encoder_sample_publish(BOARD_MOTOR_REAR_RIGHT,
                                          samples[BOARD_MOTOR_REAR_RIGHT]);
#if CRSF_REMOTE_CONTROL_ENABLE
        uint32_t now_ms = (uint32_t)xTaskGetTickCount();
        crsf_drive_mode_t mode;
        bool link_active;
        bool mode_changed;

        app_state_crsf_snapshot_copy(&crsf_input);
        app_state_grayscale_snapshot_copy(&grayscale);
        link_active = crsf_control_get_forward_speed(
            &crsf_input, now_ms, &base_speed_mm_per_s);
        mode = crsf_control_get_drive_mode(&crsf_input, now_ms);
        mode_changed = mode != last_mode;
        if (mode_changed) {
            line_control_reset(&line_control_state);
            last_mode = mode;
        }
        for (uint32_t wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
            crsf_targets[wheel] = 0.0f;
        }
        line_output = (line_control_output_t){0};
        if (!mode_changed && mode == CRSF_DRIVE_MODE_MANUAL) {
            (void)crsf_control_mix(&crsf_input, now_ms, crsf_targets);
        } else if (!mode_changed && mode == CRSF_DRIVE_MODE_LINE_TRACKING) {
            line_control_input_t line_input = {
                .line_error = grayscale.line_error,
                .line_strength = grayscale.line_strength,
                .adc_timeout_mask = grayscale.adc_timeout_mask,
                .sequence = grayscale.sequence,
                .base_speed_mm_per_s = base_speed_mm_per_s,
                .now_ms = now_ms,
            };

            line_control_step(&line_control_state, &line_input, &line_output);
            for (uint32_t wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
                crsf_targets[wheel] =
                    line_output.wheel_targets_mm_per_s[wheel];
            }
        }
        app_state_crsf_set_link_active(link_active);
        g_motor_speed_targets_mm_s[BOARD_MOTOR_FRONT_LEFT] =
            crsf_targets[BOARD_MOTOR_FRONT_LEFT];
        g_motor_speed_targets_mm_s[BOARD_MOTOR_FRONT_RIGHT] =
            crsf_targets[BOARD_MOTOR_FRONT_RIGHT];
        g_motor_speed_targets_mm_s[BOARD_MOTOR_REAR_LEFT] =
            crsf_targets[BOARD_MOTOR_REAR_LEFT];
        g_motor_speed_targets_mm_s[BOARD_MOTOR_REAR_RIGHT] =
            crsf_targets[BOARD_MOTOR_REAR_RIGHT];
#endif
        motor_control_step(samples);

#if CRSF_REMOTE_CONTROL_ENABLE
        drive_snapshot = (app_drive_control_snapshot_t){0};
        drive_snapshot.mode = mode;
        drive_snapshot.link_active = link_active;
        drive_snapshot.sb_raw = crsf_input.channels[CRSF_MODE_CHANNEL_INDEX];
        drive_snapshot.line_error = grayscale.line_error;
        drive_snapshot.line_strength = grayscale.line_strength;
        drive_snapshot.adc_timeout_mask = grayscale.adc_timeout_mask;
        drive_snapshot.line_valid = line_output.line_valid;
        drive_snapshot.lost_line_ms = line_output.lost_ms;
        drive_snapshot.base_speed_mm_per_s = base_speed_mm_per_s;
        drive_snapshot.turn_speed_mm_per_s = line_output.turn_speed_mm_per_s;
        drive_snapshot.pid_p_out = line_output.pid_p_out;
        drive_snapshot.pid_i_out = line_output.pid_i_out;
        drive_snapshot.pid_d_out = line_output.pid_d_out;
        drive_snapshot.pid_output = line_output.pid_output;
        drive_snapshot.line_sequence = grayscale.sequence;
        drive_snapshot.control_sequence = ++control_sequence;
        for (uint32_t wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
            drive_snapshot.wheel_targets_mm_per_s[wheel] =
                g_motor_speed_targets_mm_s[wheel];
            drive_snapshot.wheel_feedback_mm_per_s[wheel] =
                samples[wheel].speed_mm_per_s;
        }
        app_state_drive_control_publish(&drive_snapshot);
#endif
        app_state_encoder_cycle_end();

        board_motor_set_signed_duty(
            BOARD_MOTOR_FRONT_LEFT,
            motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT));
        board_motor_set_signed_duty(
            BOARD_MOTOR_FRONT_RIGHT,
            motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_RIGHT));
        board_motor_set_signed_duty(
            BOARD_MOTOR_REAR_LEFT,
            motor_control_get_output_duty_percent(BOARD_MOTOR_REAR_LEFT));
        board_motor_set_signed_duty(
            BOARD_MOTOR_REAR_RIGHT,
            motor_control_get_output_duty_percent(BOARD_MOTOR_REAR_RIGHT));
        vTaskDelayUntil(&last_wake_time, interval);
    }
}

void app_tasks_motor_start(void)
{
    configASSERT(xTaskCreateStatic(motor_task, "motor",
                                   APP_MOTOR_TASK_STACK_DEPTH, NULL,
                                   APP_TASK_PRIORITY, g_motor_task_stack,
                                   &g_motor_task_buffer) != NULL);
}
