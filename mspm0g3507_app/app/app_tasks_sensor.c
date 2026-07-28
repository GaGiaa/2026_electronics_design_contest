#include "app/app_tasks_sensor.h"

#include <stdbool.h>
#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#include "config/app_config.h"
#include "config/rtos_monitor_config.h"

#if APP_IMU_YAW_ENABLE
#include "algorithms/imu_fusion/imu_fusion.h"
#include "drivers/imu/board_bmi160.h"
#endif
#include "algorithms/line_tracking/line_tracking.h"
#include "app/app_state.h"
#include "config/crsf_config.h"
#include "drivers/crsf_uart/board_crsf_uart.h"
#include "drivers/grayscale/board_grayscale.h"
#include "drivers/hcsr04/board_hcsr04.h"
#include "algorithms/ultrasonic/ultrasonic_measurement.h"
#include "protocols/crsf/crsf_protocol.h"

#if APP_IMU_YAW_ENABLE
static StaticTask_t g_imu_task_buffer;
static StackType_t g_imu_task_stack[APP_IMU_TASK_STACK_DEPTH];
#endif
static StaticTask_t g_gray_task_buffer;
static StackType_t g_gray_task_stack[APP_GRAY_TASK_STACK_DEPTH];
#if CRSF_REMOTE_CONTROL_ENABLE
static StaticTask_t g_crsf_task_buffer;
static StackType_t g_crsf_task_stack[APP_CRSF_TASK_STACK_DEPTH];
#endif
#if APP_HCSR04_ENABLE
static StaticTask_t g_hcsr04_task_buffer;
static StackType_t g_hcsr04_task_stack[APP_HCSR04_TASK_STACK_DEPTH];
#endif

#if CRSF_REMOTE_CONTROL_ENABLE
static void crsf_task(void *argument)
{
    crsf_parser_t parser;
    crsf_channels_t channels;
    uint8_t byte;

    (void)argument;
    board_crsf_uart_enable_rx_interrupt();
    crsf_parser_init(&parser);
    for (;;) {
        if (board_crsf_uart_read_byte(&byte)) {
            do {
                if (crsf_parser_feed(&parser, byte, &channels)) {
                    app_state_crsf_publish(
                        &channels,
                        (uint32_t)xTaskGetTickCount(),
                        &parser,
                        board_crsf_uart_rx_overflow_count());
                }
                g_crsf_debug.crc_error_count = parser.crc_error_count;
                g_crsf_debug.frame_error_count = parser.frame_error_count;
                g_crsf_debug.rx_overflow_count =
                    board_crsf_uart_rx_overflow_count();
            } while (board_crsf_uart_read_byte(&byte));
        } else {
            vTaskDelay(pdMS_TO_TICKS(1U));
        }
    }
}
#endif

#if APP_IMU_YAW_ENABLE
static void imu_task(void *argument)
{
    TickType_t last_wake_time;
    TickType_t last_sample_tick;
    const TickType_t interval = pdMS_TO_TICKS(APP_IMU_SAMPLE_INTERVAL_MS);
    board_bmi160_sample_t sample;
    board_bmi160_status_t status;
    uint8_t chip_id;
    uint32_t consecutive_failures;
    bool has_sample_tick;
    imu_fusion_state_t fusion_state;
    app_imu_fusion_snapshot_t fusion_snapshot;

    (void)argument;
    imu_fusion_init(&fusion_state);
    has_sample_tick = false;
    for (;;) {
        chip_id = 0U;
        ++g_imu_debug.init_attempts;
        status = board_bmi160_init(&chip_id);
        g_imu_debug.chip_id = chip_id;
        g_imu_debug.initialized = status == BOARD_BMI160_STATUS_OK;
        g_imu_debug.init_status = (uint8_t)status;
        g_imu_debug.error_register = g_bmi160_diagnostics.error_register;
        g_imu_debug.pmu_status = g_bmi160_diagnostics.pmu_status;
        g_imu_debug.accel_config = g_bmi160_diagnostics.accel_config;
        g_imu_debug.accel_range = g_bmi160_diagnostics.accel_range;
        g_imu_debug.gyro_config = g_bmi160_diagnostics.gyro_config;
        g_imu_debug.gyro_range = g_bmi160_diagnostics.gyro_range;
        ++g_imu_debug.sequence;
        if (status != BOARD_BMI160_STATUS_OK) {
            app_state_imu_fusion_invalidate();
            vTaskDelay(pdMS_TO_TICKS(1000U));
        } else {
            consecutive_failures = 0U;
            imu_fusion_request_recalibration();
            has_sample_tick = false;
            last_wake_time = xTaskGetTickCount();
            for (;;) {
                TickType_t sample_tick = xTaskGetTickCount();

                ++g_imu_debug.sample_attempts;
                status = board_bmi160_read_sample(&sample);
                g_imu_debug.last_read_status = (uint8_t)status;
                if (status != BOARD_BMI160_STATUS_OK) {
                    app_state_imu_fusion_invalidate();
                    ++consecutive_failures;
                    g_imu_debug.consecutive_failures = consecutive_failures;
                    ++g_imu_debug.sequence;
                    if (consecutive_failures >= APP_IMU_REINIT_FAILURE_THRESHOLD) {
                        break;
                    }
                } else {
                    float dt_s = has_sample_tick ?
                        (float)(sample_tick - last_sample_tick) /
                            (float)configTICK_RATE_HZ :
                        (float)APP_IMU_SAMPLE_INTERVAL_MS / 1000.0f;

                    consecutive_failures = 0U;
                    ++g_imu_debug.sample_successes;
                    g_imu_debug.consecutive_failures = 0U;
                    g_imu_debug.last_sample = sample;
                    ++g_imu_debug.sequence;
                    last_sample_tick = sample_tick;
                    has_sample_tick = true;
                    imu_fusion_update(&fusion_state, &sample, dt_s);
                    fusion_snapshot.yaw_deg = fusion_state.yaw_deg;
                    fusion_snapshot.yaw_rate_dps = fusion_state.yaw_rate_dps;
                    fusion_snapshot.roll_deg = fusion_state.roll_deg;
                    fusion_snapshot.pitch_deg = fusion_state.pitch_deg;
                    fusion_snapshot.accel_norm_g = fusion_state.accel_norm_g;
                    fusion_snapshot.gyro_bias_x_dps =
                        fusion_state.gyro_bias_x_dps;
                    fusion_snapshot.gyro_bias_y_dps =
                        fusion_state.gyro_bias_y_dps;
                    fusion_snapshot.gyro_bias_z_dps =
                        fusion_state.gyro_bias_z_dps;
                    fusion_snapshot.dt_s = fusion_state.dt_s;
                    fusion_snapshot.acceleration_valid =
                        fusion_state.acceleration_valid;
                    fusion_snapshot.stationary_confirmed =
                        fusion_state.stationary_confirmed;
                    fusion_snapshot.calibrated = fusion_state.calibrated;
                    fusion_snapshot.valid = fusion_state.calibrated;
                    app_state_imu_fusion_publish(&fusion_snapshot);
                }
                vTaskDelayUntil(&last_wake_time, interval);
            }
        }
    }
}
#endif

static void gray_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(APP_GRAY_SAMPLE_INTERVAL_MS);
    board_grayscale_snapshot_t sample;
    line_tracking_result_t tracking;

    (void)argument;
    for (;;) {
        board_grayscale_sample(&sample);
        line_tracking_update(&g_line_tracking_state, sample.normalized,
                             sample.digital, &tracking);
        sample.black_mask = tracking.black_mask;
        sample.black_count = tracking.black_count;
        sample.line_strength = tracking.line_strength;
        sample.line_error = tracking.line_error;
        app_state_grayscale_publish(&sample);
        vTaskDelayUntil(&last_wake_time, interval);
    }
}

#if APP_HCSR04_ENABLE
static void hcsr04_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(APP_HCSR04_SAMPLE_INTERVAL_MS);
    board_hcsr04_result_t result;
    app_hcsr04_snapshot_t snapshot = {0};
    TaskHandle_t task_handle = xTaskGetCurrentTaskHandle();

    (void)argument;
    board_hcsr04_set_task_handle(task_handle);
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, 0U);
        board_hcsr04_trigger();
        if (ulTaskNotifyTake(pdTRUE,
                             pdMS_TO_TICKS(APP_HCSR04_ECHO_TIMEOUT_MS)) == 0U) {
            board_hcsr04_abort();
            snapshot.valid = false;
            snapshot.distance_mm = 0U;
            snapshot.echo_time_us = 0U;
            ++snapshot.timeout_count;
        } else {
            board_hcsr04_read_result(&result);
            snapshot.valid = result.valid && ultrasonic_measurement_calculate(
                result.echo_ticks, RTOS_MONITOR_TIMER_HZ, 100U, 30000U,
                &snapshot.echo_time_us, &snapshot.distance_mm);
        }
        ++snapshot.sequence;
        app_state_hcsr04_publish(&snapshot);
        vTaskDelayUntil(&last_wake_time, interval);
    }
}
#endif

void app_tasks_sensor_start(void)
{
#if CRSF_REMOTE_CONTROL_ENABLE
    configASSERT(xTaskCreateStatic(crsf_task, "crsf", APP_CRSF_TASK_STACK_DEPTH,
                                   NULL, APP_TASK_PRIORITY, g_crsf_task_stack,
                                   &g_crsf_task_buffer) != NULL);
#endif
#if APP_IMU_YAW_ENABLE
    configASSERT(xTaskCreateStatic(imu_task, "imu", APP_IMU_TASK_STACK_DEPTH, NULL,
                                   APP_IMU_TASK_PRIORITY, g_imu_task_stack,
                                   &g_imu_task_buffer) != NULL);
#endif
    configASSERT(xTaskCreateStatic(gray_task, "gray", APP_GRAY_TASK_STACK_DEPTH,
                                   NULL, APP_TELEMETRY_TASK_PRIORITY,
                                   g_gray_task_stack, &g_gray_task_buffer) != NULL);
#if APP_HCSR04_ENABLE
    board_hcsr04_init();
    configASSERT(xTaskCreateStatic(hcsr04_task, "hcsr04", APP_HCSR04_TASK_STACK_DEPTH,
                                   NULL, APP_TASK_PRIORITY, g_hcsr04_task_stack,
                                   &g_hcsr04_task_buffer) != NULL);
#endif
}
