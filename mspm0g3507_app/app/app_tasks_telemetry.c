#include "app/app_tasks_telemetry.h"

#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#include "algorithms/motor_control/motor_control.h"
#include "app/app_state.h"
#include "config/app_config.h"
#include "drivers/grayscale/board_grayscale.h"
#include "drivers/uart/board_uart.h"
#include "protocols/vofa/vofa_justfloat.h"
#include "services/rtos_monitor/rtos_monitor.h"

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
#if APP_IMU_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(
                     imu_vofa_task, "imu_vofa",
                     APP_IMU_VOFA_TELEMETRY_TASK_STACK_DEPTH, NULL,
                     APP_TELEMETRY_TASK_PRIORITY, g_imu_vofa_task_stack,
                     &g_imu_vofa_task_buffer) != NULL);
#endif
#if APP_VOFA_SPEED_PID_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(
                     vofa_speed_pid_telemetry_task, "vofa_speed_pid",
                     APP_VOFA_SPEED_PID_TELEMETRY_TASK_STACK_DEPTH, NULL,
                     APP_VOFA_SPEED_PID_TELEMETRY_TASK_PRIORITY,
                     g_vofa_speed_pid_telemetry_task_stack,
                     &g_vofa_speed_pid_telemetry_task_buffer) != NULL);
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
