#include "app/app_tasks_sensor.h"

#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#include "config/app_config.h"

#if APP_IMU_YAW_ENABLE
#include "algorithms/imu_yaw/board_imu_yaw.h"
#include "drivers/encoder/board_encoder.h"
#include "drivers/imu/board_bmi160.h"
#endif
#include "algorithms/line_tracking/line_tracking.h"
#include "app/app_state.h"
#include "config/crsf_config.h"
#include "drivers/crsf_uart/board_crsf_uart.h"
#include "drivers/grayscale/board_grayscale.h"
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
    const TickType_t interval = pdMS_TO_TICKS(APP_IMU_SAMPLE_INTERVAL_MS);
    board_bmi160_sample_t sample;
    board_bmi160_status_t status;
    uint8_t chip_id;
    uint32_t consecutive_failures;
#if APP_IMU_YAW_ENABLE
    board_imu_yaw_state_t yaw_state;
    board_encoder_sample_t encoder_samples[BOARD_MOTOR_COUNT];
#endif

    (void)argument;
    board_imu_yaw_init(&yaw_state, APP_IMU_YAW_TRACK_WIDTH_MM);
    for (;;) {
        chip_id = 0U;
        status = board_bmi160_init(&chip_id);
        if (status != BOARD_BMI160_STATUS_OK) {
            app_state_imu_yaw_invalidate();
            vTaskDelay(pdMS_TO_TICKS(1000U));
        } else {
            consecutive_failures = 0U;
            last_wake_time = xTaskGetTickCount();
            for (;;) {
                status = board_bmi160_read_sample(&sample);
                if (status != BOARD_BMI160_STATUS_OK) {
                    app_state_imu_yaw_invalidate();
                    ++consecutive_failures;
                    if (consecutive_failures >= APP_IMU_REINIT_FAILURE_THRESHOLD) {
                        break;
                    }
                } else {
                    consecutive_failures = 0U;
                    app_state_encoder_samples_snapshot_copy(encoder_samples);
                    board_imu_yaw_update(&yaw_state, &sample, encoder_samples,
                                         (float)APP_IMU_SAMPLE_INTERVAL_MS / 1000.0f);
                    app_state_imu_yaw_publish(yaw_state.yaw_deg,
                                              yaw_state.yaw_rate_dps,
                                              yaw_state.gyro_bias_z_dps);
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
}
