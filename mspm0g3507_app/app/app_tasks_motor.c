#include "app/app_tasks_motor.h"

#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#include "config/app_config.h"
#include "algorithms/course_following/course_following.h"
#if APP_IMU_YAW_ENABLE
#include "algorithms/imu_yaw/board_imu_yaw.h"
#endif
#include "algorithms/line_control/line_control.h"
#include "algorithms/motor_control/motor_control.h"
#include "algorithms/yaw_control/yaw_control.h"
#include "app/app_state.h"
#include "config/course_following_config.h"
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
    yaw_control_state_t yaw_control_state;
    yaw_control_output_t yaw_output;
    course_following_state_t course_following_state;
    course_following_output_t course_output;
#if APP_IMU_YAW_ENABLE
    app_imu_yaw_snapshot_t imu_yaw_snapshot;
    bool course_imu_invalid_seen;
#endif
    app_drive_control_snapshot_t drive_snapshot;
    float crsf_targets[BOARD_MOTOR_COUNT];
    float base_speed_mm_per_s;
    uint32_t control_sequence = 0U;
    crsf_drive_mode_t last_mode = CRSF_DRIVE_MODE_IDLE;
#endif

    (void)argument;
#if CRSF_REMOTE_CONTROL_ENABLE
    line_control_init(&line_control_state);
    yaw_control_init(&yaw_control_state);
    course_following_init(&course_following_state);
#if APP_IMU_YAW_ENABLE
    course_imu_invalid_seen = false;
#endif
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
#if APP_IMU_YAW_ENABLE
        app_state_imu_yaw_snapshot_copy(&imu_yaw_snapshot);
#endif
        link_active = crsf_control_get_forward_speed(
            &crsf_input, now_ms, &base_speed_mm_per_s);
        mode = crsf_control_get_drive_mode(&crsf_input, now_ms);
        mode_changed = mode != last_mode;
        if (mode_changed) {
            line_control_reset(&line_control_state);
            yaw_control_reset(&yaw_control_state);
            course_following_reset(&course_following_state);
#if APP_IMU_YAW_ENABLE
            if (mode == CRSF_DRIVE_MODE_COURSE_FOLLOWING) {
                board_imu_yaw_request_recalibration();
                course_imu_invalid_seen = false;
            }
#endif
            last_mode = mode;
        }
        for (uint32_t wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
            crsf_targets[wheel] = 0.0f;
        }
        line_output = (line_control_output_t){0};
        yaw_output = (yaw_control_output_t){0};
        course_output = (course_following_output_t){0};
        if (!mode_changed && mode == CRSF_DRIVE_MODE_MANUAL) {
            (void)crsf_control_mix(&crsf_input, now_ms, crsf_targets);
        } else if (!mode_changed && mode == CRSF_DRIVE_MODE_YAW_HOLD) {
#if APP_IMU_YAW_ENABLE
            yaw_control_input_t yaw_input = {
                .feedback_yaw_deg = imu_yaw_snapshot.yaw_deg,
                .feedback_valid = imu_yaw_snapshot.valid,
                .base_speed_mm_per_s = base_speed_mm_per_s,
                .now_ms = now_ms,
            };

            yaw_control_step(&yaw_control_state, &yaw_input, &yaw_output);
            for (uint32_t wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
                crsf_targets[wheel] =
                    yaw_output.wheel_targets_mm_per_s[wheel];
            }
#else
            yaw_control_reset(&yaw_control_state);
#endif
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
        } else if (!mode_changed && mode == CRSF_DRIVE_MODE_COURSE_FOLLOWING) {
#if APP_IMU_YAW_ENABLE
            if (!course_imu_invalid_seen) {
                course_imu_invalid_seen = !imu_yaw_snapshot.valid;
            } else if (imu_yaw_snapshot.valid) {
                course_following_input_t course_input = {
                    .grayscale = &grayscale,
                    .yaw_valid = true,
                    .yaw_deg = imu_yaw_snapshot.yaw_deg,
                    .now_ms = now_ms,
                };

                course_following_step(&course_following_state, &course_input,
                                      &course_output);
                for (uint32_t wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
                    crsf_targets[wheel] =
                        course_output.wheel_targets_mm_per_s[wheel];
                }
            }
#endif
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
        drive_snapshot.sc_raw = crsf_input.channels[CRSF_SC_CHANNEL_INDEX];
        drive_snapshot.line_error = grayscale.line_error;
        drive_snapshot.line_strength = grayscale.line_strength;
        drive_snapshot.adc_timeout_mask = grayscale.adc_timeout_mask;
        drive_snapshot.line_valid =
            mode == CRSF_DRIVE_MODE_COURSE_FOLLOWING ?
                (!course_output.line_lost && !course_output.all_black_stop) :
                line_output.line_valid;
        drive_snapshot.lost_line_ms = line_output.lost_ms;
        drive_snapshot.base_speed_mm_per_s =
            mode == CRSF_DRIVE_MODE_COURSE_FOLLOWING ?
                COURSE_FOLLOWING_BASE_SPEED_MM_PER_S : base_speed_mm_per_s;
        if (mode == CRSF_DRIVE_MODE_YAW_HOLD) {
            drive_snapshot.turn_speed_mm_per_s =
                yaw_output.turn_speed_mm_per_s;
            drive_snapshot.pid_p_out = yaw_output.pid_p_out;
            drive_snapshot.pid_i_out = yaw_output.pid_i_out;
            drive_snapshot.pid_d_out = yaw_output.pid_d_out;
            drive_snapshot.pid_output = yaw_output.pid_output;
        } else if (mode == CRSF_DRIVE_MODE_COURSE_FOLLOWING) {
            drive_snapshot.turn_speed_mm_per_s =
                course_output.heading_hold ? course_output.heading_pid_output :
                                             course_output.line_pid_output;
            drive_snapshot.pid_output = drive_snapshot.turn_speed_mm_per_s;
        } else {
            drive_snapshot.turn_speed_mm_per_s =
                line_output.turn_speed_mm_per_s;
            drive_snapshot.pid_p_out = line_output.pid_p_out;
            drive_snapshot.pid_i_out = line_output.pid_i_out;
            drive_snapshot.pid_d_out = line_output.pid_d_out;
            drive_snapshot.pid_output = line_output.pid_output;
        }
        drive_snapshot.yaw_valid = yaw_output.yaw_valid;
        drive_snapshot.yaw_target_deg = g_yaw_control_debug.target_yaw_deg;
#if APP_IMU_YAW_ENABLE
        drive_snapshot.yaw_feedback_deg = imu_yaw_snapshot.yaw_deg;
        if (mode == CRSF_DRIVE_MODE_COURSE_FOLLOWING) {
            drive_snapshot.yaw_valid = imu_yaw_snapshot.valid &&
                                      course_imu_invalid_seen;
        }
#endif
        drive_snapshot.yaw_error_deg = yaw_output.yaw_error_deg;
        drive_snapshot.course_heading_hold = course_output.heading_hold;
        drive_snapshot.course_heading_target_deg =
            course_output.heading_target_deg;
        if (mode == CRSF_DRIVE_MODE_COURSE_FOLLOWING) {
            drive_snapshot.yaw_target_deg = course_output.heading_target_deg;
            drive_snapshot.yaw_error_deg = course_output.heading_error_deg;
        }
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
