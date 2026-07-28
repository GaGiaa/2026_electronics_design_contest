#include "app/app_tasks_telemetry.h"

#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#include "algorithms/motor_control/motor_control.h"
#include "app/app_state.h"
#include "config/app_config.h"
#include "drivers/buttons/board_buttons.h"
#include "drivers/grayscale/board_grayscale.h"
#include "drivers/hcsr04/board_hcsr04.h"
#include "drivers/uart/board_uart.h"
#include "protocols/vofa/course_following_telemetry.h"
#include "protocols/vofa/vofa_justfloat.h"
#include "services/rtos_monitor/rtos_monitor.h"

#if APP_BUTTON_VOFA_TELEMETRY_ENABLE
static StaticTask_t g_button_vofa_task_buffer;
static StackType_t g_button_vofa_task_stack[
    APP_BUTTON_VOFA_TELEMETRY_TASK_STACK_DEPTH];

static void button_vofa_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval =
        pdMS_TO_TICKS(APP_BUTTON_VOFA_TELEMETRY_INTERVAL_MS);
    app_button_snapshot_t snapshot;
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(BOARD_BUTTON_COUNT)];

    (void)argument;
    for (;;) {
        app_state_buttons_snapshot_copy(&snapshot);
        if (vofa_justfloat_encode4(
                frame, sizeof(frame),
                (snapshot.pressed_mask & (1U << BOARD_BUTTON_PA7)) != 0U
                    ? 1.0f
                    : 0.0f,
                (snapshot.pressed_mask & (1U << BOARD_BUTTON_PB12)) != 0U
                    ? 1.0f
                    : 0.0f,
                (snapshot.pressed_mask & (1U << BOARD_BUTTON_PA8)) != 0U
                    ? 1.0f
                    : 0.0f,
                (snapshot.pressed_mask & (1U << BOARD_BUTTON_PA30)) != 0U
                    ? 1.0f
                    : 0.0f)) {
            board_uart_write(frame, sizeof(frame));
        }
        vTaskDelayUntil(&last_wake_time, interval);
    }
}
#endif

#if APP_COURSE_FOLLOWING_VOFA_TELEMETRY_ENABLE
static StaticTask_t g_course_following_vofa_task_buffer;
static StackType_t g_course_following_vofa_task_stack[
    APP_COURSE_FOLLOWING_VOFA_TELEMETRY_TASK_STACK_DEPTH];

static void course_following_vofa_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval =
        pdMS_TO_TICKS(APP_COURSE_FOLLOWING_VOFA_TELEMETRY_INTERVAL_MS);
    app_drive_control_snapshot_t drive;
    app_imu_yaw_snapshot_t imu;
    motor_control_wheel_status_t control[BOARD_MOTOR_COUNT];
    course_following_telemetry_values_t values;
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(
        COURSE_FOLLOWING_TELEMETRY_CHANNEL_COUNT)];
    uint32_t wheel;

    (void)argument;
    for (;;) {
        app_state_drive_control_snapshot_copy(&drive);
        app_state_imu_yaw_snapshot_copy(&imu);
        app_state_motor_control_snapshot_copy(control);
        values.yaw_deg = imu.yaw_deg;
        values.yaw_rate_dps = imu.yaw_rate_dps;
        values.gyro_bias_z_dps = imu.gyro_bias_z_dps;
        values.heading_target_deg = drive.course_heading_target_deg;
        values.heading_hold = drive.course_heading_hold;
        values.line_error = drive.line_error;
        for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
            values.target_speed_mm_per_s[wheel] =
                drive.wheel_targets_mm_per_s[wheel];
            values.feedback_speed_mm_per_s[wheel] =
                control[wheel].feedback_speed_mm_per_s;
        }
        if (course_following_telemetry_encode(frame, sizeof(frame), &values)) {
            board_uart_write(frame, sizeof(frame));
        }
        vTaskDelayUntil(&last_wake_time, interval);
    }
}
#endif

#if APP_IMU_TELEMETRY_ENABLE
static StaticTask_t g_imu_vofa_task_buffer;
static StackType_t g_imu_vofa_task_stack[
    APP_IMU_VOFA_TELEMETRY_TASK_STACK_DEPTH];

static void imu_vofa_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval =
        pdMS_TO_TICKS(APP_IMU_VOFA_TELEMETRY_INTERVAL_MS);
    app_imu_yaw_snapshot_t snapshot;
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(3U)];

    (void)argument;
    for (;;) {
        app_state_imu_yaw_snapshot_copy(&snapshot);
        if (snapshot.valid &&
            vofa_justfloat_encode3(frame, sizeof(frame), snapshot.yaw_deg,
                                    snapshot.yaw_rate_dps,
                                    snapshot.gyro_bias_z_dps)) {
            board_uart_write(frame, sizeof(frame));
        }
        vTaskDelayUntil(&last_wake_time, interval);
    }
}
#endif

#if APP_HCSR04_TELEMETRY_ENABLE
static StaticTask_t g_hcsr04_vofa_task_buffer;
static StackType_t g_hcsr04_vofa_task_stack[APP_HCSR04_TASK_STACK_DEPTH];

static void hcsr04_vofa_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(APP_HCSR04_SAMPLE_INTERVAL_MS);
    app_hcsr04_snapshot_t snapshot;
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(3U)];

    (void)argument;
    for (;;) {
        app_state_hcsr04_snapshot_copy(&snapshot);
        if (vofa_justfloat_encode3(
                frame, sizeof(frame), (float)snapshot.distance_mm,
                (float)snapshot.echo_time_us,
                snapshot.valid ? 1.0f : 0.0f)) {
            board_uart_write(frame, sizeof(frame));
        }
        vTaskDelayUntil(&last_wake_time, interval);
    }
}
#endif

#if APP_VOFA_SPEED_PID_TELEMETRY_ENABLE
static StaticTask_t g_vofa_speed_pid_telemetry_task_buffer;
static StackType_t g_vofa_speed_pid_telemetry_task_stack[
    APP_VOFA_SPEED_PID_TELEMETRY_TASK_STACK_DEPTH];

static void vofa_speed_pid_telemetry_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval =
        pdMS_TO_TICKS(APP_VOFA_SPEED_PID_TELEMETRY_INTERVAL_MS);
    motor_control_wheel_status_t control[BOARD_MOTOR_COUNT];
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(VOFA_JUSTFLOAT_CHANNEL_COUNT)];

    (void)argument;
    for (;;) {
        board_motor_wheel_t wheel = g_motor_debug.wheel;

        app_state_motor_control_snapshot_copy(control);
        if (wheel >= BOARD_MOTOR_COUNT) {
            wheel = BOARD_MOTOR_FRONT_LEFT;
        }
        if (vofa_justfloat_encode4(
                frame, sizeof(frame), control[wheel].target_speed_mm_per_s,
                control[wheel].instant_feedback_speed_mm_per_s,
                control[wheel].feedback_speed_mm_per_s,
                control[wheel].output_duty_percent)) {
            board_uart_write(frame, sizeof(frame));
        }
        vTaskDelayUntil(&last_wake_time, interval);
    }
}
#endif

#if APP_LINE_CONTROL_VOFA_TELEMETRY_ENABLE
static StaticTask_t g_line_control_vofa_task_buffer;
static StackType_t g_line_control_vofa_task_stack[
    APP_LINE_CONTROL_VOFA_TELEMETRY_TASK_STACK_DEPTH];

static void line_control_vofa_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval =
        pdMS_TO_TICKS(APP_LINE_CONTROL_VOFA_TELEMETRY_INTERVAL_MS);
    app_drive_control_snapshot_t drive;
    motor_control_wheel_status_t control[BOARD_MOTOR_COUNT];
    float channels[APP_LINE_CONTROL_CHANNEL_COUNT];
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(APP_LINE_CONTROL_CHANNEL_COUNT)];

    (void)argument;
    for (;;) {
        app_state_drive_control_snapshot_copy(&drive);
        app_state_motor_control_snapshot_copy(control);

        channels[0U] = (float)drive.mode;
        channels[1U] = drive.link_active ? 1.0f : 0.0f;
        channels[2U] = (float)drive.line_error;
        channels[3U] = (float)drive.line_strength;
        channels[4U] = drive.line_valid ? 1.0f : 0.0f;
        channels[5U] = (float)drive.lost_line_ms;
        channels[6U] = (float)drive.adc_timeout_mask;
        channels[7U] = drive.base_speed_mm_per_s;
        channels[8U] = drive.turn_speed_mm_per_s;
        channels[9U] =
            (drive.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT] +
             drive.wheel_targets_mm_per_s[BOARD_MOTOR_REAR_LEFT]) * 0.5f;
        channels[10U] =
            (drive.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_RIGHT] +
             drive.wheel_targets_mm_per_s[BOARD_MOTOR_REAR_RIGHT]) * 0.5f;
        channels[11U] =
            (control[BOARD_MOTOR_FRONT_LEFT].feedback_speed_mm_per_s +
             control[BOARD_MOTOR_REAR_LEFT].feedback_speed_mm_per_s) * 0.5f;
        channels[12U] =
            (control[BOARD_MOTOR_FRONT_RIGHT].feedback_speed_mm_per_s +
             control[BOARD_MOTOR_REAR_RIGHT].feedback_speed_mm_per_s) * 0.5f;
        channels[13U] =
            (control[BOARD_MOTOR_FRONT_LEFT].output_duty_percent +
             control[BOARD_MOTOR_REAR_LEFT].output_duty_percent) * 0.5f;
        channels[14U] =
            (control[BOARD_MOTOR_FRONT_RIGHT].output_duty_percent +
             control[BOARD_MOTOR_REAR_RIGHT].output_duty_percent) * 0.5f;
        channels[15U] = drive.pid_p_out;
        channels[16U] = drive.pid_i_out;
        channels[17U] = drive.pid_d_out;

        if (vofa_justfloat_encode(frame, sizeof(frame), channels,
                                  APP_LINE_CONTROL_CHANNEL_COUNT)) {
            board_uart_write(frame, sizeof(frame));
        }
        vTaskDelayUntil(&last_wake_time, interval);
    }
}
#endif

#if APP_GRAY_VOFA_TELEMETRY_ENABLE
static StaticTask_t g_gray_telemetry_task_buffer;
static StackType_t g_gray_telemetry_task_stack[APP_GRAY_TASK_STACK_DEPTH];

static void gray_vofa_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval =
        pdMS_TO_TICKS(APP_GRAY_VOFA_TELEMETRY_INTERVAL_MS);
    board_grayscale_snapshot_t snapshot;
    float channels[APP_GRAY_VOFA_CHANNEL_COUNT];
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(APP_GRAY_VOFA_CHANNEL_COUNT)];
    uint32_t channel;

    (void)argument;
    for (;;) {
        app_state_grayscale_snapshot_copy(&snapshot);
        for (channel = 0U; channel < BOARD_GRAYSCALE_CHANNEL_COUNT; ++channel) {
            channels[channel] = (float)snapshot.raw[channel];
            channels[BOARD_GRAYSCALE_CHANNEL_COUNT + channel] =
                (float)snapshot.normalized[channel];
        }
        channels[16U] = (float)snapshot.digital;
        channels[17U] = (float)snapshot.black_mask;
        channels[18U] = (float)snapshot.black_count;
        channels[19U] = (float)snapshot.line_error;
        channels[20U] = (float)snapshot.line_strength;
        channels[21U] = (float)snapshot.sequence;
        if (vofa_justfloat_encode(frame, sizeof(frame), channels,
                                  APP_GRAY_VOFA_CHANNEL_COUNT)) {
            board_uart_write(frame, sizeof(frame));
        }
        vTaskDelayUntil(&last_wake_time, interval);
    }
}
#endif

#if APP_RTOS_MONITOR_ENABLE
static StaticTask_t g_rtos_monitor_task_buffer;
static StackType_t g_rtos_monitor_task_stack[APP_RTOS_MONITOR_TASK_STACK_DEPTH];
#endif

void app_tasks_telemetry_start(void)
{
#if APP_COURSE_FOLLOWING_VOFA_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(
                     course_following_vofa_task, "course_following_vofa",
                     APP_COURSE_FOLLOWING_VOFA_TELEMETRY_TASK_STACK_DEPTH, NULL,
                     APP_TELEMETRY_TASK_PRIORITY,
                     g_course_following_vofa_task_stack,
                     &g_course_following_vofa_task_buffer) != NULL);
#endif
#if APP_BUTTON_VOFA_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(
                     button_vofa_task, "button_vofa",
                     APP_BUTTON_VOFA_TELEMETRY_TASK_STACK_DEPTH, NULL,
                     APP_TELEMETRY_TASK_PRIORITY, g_button_vofa_task_stack,
                     &g_button_vofa_task_buffer) != NULL);
#endif
#if APP_IMU_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(
                     imu_vofa_task, "imu_vofa",
                     APP_IMU_VOFA_TELEMETRY_TASK_STACK_DEPTH, NULL,
                     APP_TELEMETRY_TASK_PRIORITY, g_imu_vofa_task_stack,
                     &g_imu_vofa_task_buffer) != NULL);
#endif
#if APP_HCSR04_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(
                     hcsr04_vofa_task, "hcsr04_vofa",
                     APP_HCSR04_TASK_STACK_DEPTH, NULL,
                     APP_TELEMETRY_TASK_PRIORITY, g_hcsr04_vofa_task_stack,
                     &g_hcsr04_vofa_task_buffer) != NULL);
#endif
#if APP_VOFA_SPEED_PID_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(
                     vofa_speed_pid_telemetry_task, "vofa_speed_pid",
                     APP_VOFA_SPEED_PID_TELEMETRY_TASK_STACK_DEPTH, NULL,
                     APP_VOFA_SPEED_PID_TELEMETRY_TASK_PRIORITY,
                     g_vofa_speed_pid_telemetry_task_stack,
                     &g_vofa_speed_pid_telemetry_task_buffer) != NULL);
#endif
#if APP_LINE_CONTROL_VOFA_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(
                     line_control_vofa_task, "line_control_vofa",
                     APP_LINE_CONTROL_VOFA_TELEMETRY_TASK_STACK_DEPTH, NULL,
                     APP_TELEMETRY_TASK_PRIORITY, g_line_control_vofa_task_stack,
                     &g_line_control_vofa_task_buffer) != NULL);
#endif
#if APP_GRAY_VOFA_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(
                     gray_vofa_task, "gray_vofa", APP_GRAY_TASK_STACK_DEPTH, NULL,
                     APP_TELEMETRY_TASK_PRIORITY, g_gray_telemetry_task_stack,
                     &g_gray_telemetry_task_buffer) != NULL);
#endif
#if APP_RTOS_MONITOR_ENABLE
    configASSERT(xTaskCreateStatic(
                     rtos_monitor_task, "rtos_monitor",
                     APP_RTOS_MONITOR_TASK_STACK_DEPTH, NULL,
                     APP_TELEMETRY_TASK_PRIORITY, g_rtos_monitor_task_stack,
                     &g_rtos_monitor_task_buffer) != NULL);
#endif
}
