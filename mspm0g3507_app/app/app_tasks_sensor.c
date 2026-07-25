#include "app/app_tasks_sensor.h"

#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#include "algorithms/imu_yaw/board_imu_yaw.h"
#include "algorithms/line_tracking/line_tracking.h"
#include "app/app_profile.h"
#include "app/app_state.h"
#include "drivers/crsf_uart/board_crsf_uart.h"
#include "drivers/encoder/board_encoder.h"
#include "drivers/grayscale/board_grayscale.h"
#include "drivers/imu/board_bmi160.h"
#include "drivers/uart/board_uart.h"
#include "protocols/crsf/crsf_protocol.h"
#include "protocols/vofa/vofa_justfloat.h"

static StaticTask_t g_imu_task_buffer;
static StackType_t g_imu_task_stack[IMU_TASK_STACK_DEPTH];
static StaticTask_t g_gray_task_buffer;
static StackType_t g_gray_task_stack[GRAY_TASK_STACK_DEPTH];
#if CRSF_REMOTE_CONTROL_ENABLE
static StaticTask_t g_crsf_task_buffer;
static StackType_t g_crsf_task_stack[CRSF_TASK_STACK_DEPTH];
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

static void imu_task(void *argument)
{
    TickType_t last_wake_time;
    const TickType_t interval = pdMS_TO_TICKS(IMU_SAMPLE_INTERVAL_MS);
    board_bmi160_sample_t sample;
    board_bmi160_status_t status;
    uint8_t chip_id;
    uint32_t consecutive_failures;
#if IMU_YAW_ENABLE
    board_imu_yaw_state_t yaw_state;
    board_encoder_sample_t encoder_samples[BOARD_MOTOR_COUNT];
#endif
#if IMU_TELEMETRY_ENABLE && IMU_YAW_ENABLE
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(3U)];
#endif

    (void)argument;
#if IMU_YAW_ENABLE
    board_imu_yaw_init(&yaw_state, IMU_YAW_TRACK_WIDTH_MM);
#endif
    for (;;) {
        chip_id = 0U;
        status = board_bmi160_init(&chip_id);
        if (status != BOARD_BMI160_STATUS_OK) {
            vTaskDelay(pdMS_TO_TICKS(1000U));
        } else {
            consecutive_failures = 0U;
            last_wake_time = xTaskGetTickCount();
            for (;;) {
                status = board_bmi160_read_sample(&sample);
                if (status != BOARD_BMI160_STATUS_OK) {
                    ++consecutive_failures;
                    if (consecutive_failures >= IMU_REINIT_FAILURE_THRESHOLD) {
                        break;
                    }
                } else {
                    consecutive_failures = 0U;
#if IMU_YAW_ENABLE
                    app_state_encoder_samples_snapshot_copy(encoder_samples);
                    board_imu_yaw_update(&yaw_state, &sample, encoder_samples,
                                         (float)IMU_SAMPLE_INTERVAL_MS / 1000.0f);
#endif
#if IMU_TELEMETRY_ENABLE && IMU_YAW_ENABLE
                    if (vofa_justfloat_encode3(
                            frame, sizeof(frame), yaw_state.yaw_deg,
                            yaw_state.yaw_rate_dps, yaw_state.gyro_bias_z_dps)) {
                        board_uart_write(frame, sizeof(frame));
                    }
#endif
                }
                vTaskDelayUntil(&last_wake_time, interval);
            }
        }
    }
}

static void gray_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(GRAY_SAMPLE_INTERVAL_MS);
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
    configASSERT(xTaskCreateStatic(crsf_task, "crsf", CRSF_TASK_STACK_DEPTH,
                                   NULL, APP_TASK_PRIORITY, g_crsf_task_stack,
                                   &g_crsf_task_buffer) != NULL);
#endif
    configASSERT(xTaskCreateStatic(imu_task, "imu", IMU_TASK_STACK_DEPTH, NULL,
                                   IMU_TASK_PRIORITY, g_imu_task_stack,
                                   &g_imu_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(gray_task, "gray", GRAY_TASK_STACK_DEPTH,
                                   NULL, TELEMETRY_TASK_PRIORITY,
                                   g_gray_task_stack, &g_gray_task_buffer) != NULL);
}
