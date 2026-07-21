#include <stdint.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "board_encoder.h"
#include "board_bmi160.h"
#include "board_buzzer.h"
#include "board_motor.h"
#include "board_uart.h"
#include "board_ws2812.h"
#include "motor_control.h"
#include "ti_msp_dl_config.h"

#define APP_TASK_PRIORITY 1U
#define MOTOR_TASK_STACK_DEPTH 256U
#define WS2812_TASK_STACK_DEPTH 256U
#define UART_TASK_STACK_DEPTH 256U
#define UART_TX_TASK_STACK_DEPTH 256U
#define BUZZER_TASK_STACK_DEPTH 128U
#define UART_RX_QUEUE_LENGTH 64U
#define WS2812_BRIGHTNESS 16U
#define ENCODER_TELEMETRY_ENABLE 0U
#define ENCODER_TELEMETRY_INTERVAL_MS 100U
#define ENCODER_TELEMETRY_TASK_STACK_DEPTH 512U
#define TELEMETRY_TASK_PRIORITY 0U
#define IMU_TELEMETRY_ENABLE 0U
#define IMU_TASK_STACK_DEPTH 512U
#define IMU_TASK_PRIORITY 0U
#define IMU_SAMPLE_INTERVAL_MS 10U
#define IMU_REINIT_FAILURE_THRESHOLD 3U

#define BUZZER_FEATURE_ENABLE 0U
#define BUZZER_FREQUENCY_HZ 2000U
#define BUZZER_DUTY_PERCENT 50U
#define BUZZER_ON_TIME_MS 200U
#define BUZZER_OFF_TIME_MS 1800U

#if (BUZZER_FREQUENCY_HZ < 1000U) || (BUZZER_FREQUENCY_HZ > 20000U)
#error "BUZZER_FREQUENCY_HZ must be between 1000 Hz and 20000 Hz"
#endif

#if (BUZZER_ON_TIME_MS == 0U) || (BUZZER_OFF_TIME_MS == 0U)
#error "BUZZER_ON_TIME_MS and BUZZER_OFF_TIME_MS must be nonzero"
#endif

static StaticTask_t g_motor_task_buffer;
static StackType_t g_motor_task_stack[MOTOR_TASK_STACK_DEPTH];
static StaticTask_t g_ws2812_task_buffer;
static StackType_t g_ws2812_task_stack[WS2812_TASK_STACK_DEPTH];
static StaticTask_t g_uart_task_buffer;
static StackType_t g_uart_task_stack[UART_TASK_STACK_DEPTH];
static StaticTask_t g_uart_tx_task_buffer;
static StackType_t g_uart_tx_task_stack[UART_TX_TASK_STACK_DEPTH];
#if ENCODER_TELEMETRY_ENABLE
static StaticTask_t g_telemetry_task_buffer;
static StackType_t g_telemetry_task_stack[ENCODER_TELEMETRY_TASK_STACK_DEPTH];
#endif
static StaticTask_t g_imu_task_buffer;
static StackType_t g_imu_task_stack[IMU_TASK_STACK_DEPTH];
#if BUZZER_FEATURE_ENABLE
static StaticTask_t g_buzzer_task_buffer;
static StackType_t g_buzzer_task_stack[BUZZER_TASK_STACK_DEPTH];
#endif
static StaticQueue_t g_uart_queue_buffer;
static uint8_t g_uart_queue_storage[UART_RX_QUEUE_LENGTH * sizeof(uint8_t)];
static StaticTask_t g_idle_task_buffer;
static StackType_t g_idle_task_stack[configIDLE_TASK_STACK_DEPTH];
volatile board_encoder_sample_t g_encoder_samples[BOARD_MOTOR_COUNT];
static volatile uint32_t g_encoder_sample_sequence;
void UART_0_INST_IRQHandler(void) { board_uart_irq_handler(); }
void GROUP1_IRQHandler(void) { board_encoder_gpioa_irq_handler(); }

static void motor_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(10U);
    board_encoder_sample_t samples[BOARD_MOTOR_COUNT];

    (void)argument;
    for (;;) {
        ++g_encoder_sample_sequence;
        samples[BOARD_MOTOR_FRONT_LEFT] = board_encoder_sample(BOARD_MOTOR_FRONT_LEFT);
        samples[BOARD_MOTOR_FRONT_RIGHT] = board_encoder_sample(BOARD_MOTOR_FRONT_RIGHT);
        samples[BOARD_MOTOR_REAR_LEFT] = board_encoder_sample(BOARD_MOTOR_REAR_LEFT);
        samples[BOARD_MOTOR_REAR_RIGHT] = board_encoder_sample(BOARD_MOTOR_REAR_RIGHT);
        g_encoder_samples[BOARD_MOTOR_FRONT_LEFT] = samples[BOARD_MOTOR_FRONT_LEFT];
        g_encoder_samples[BOARD_MOTOR_FRONT_RIGHT] = samples[BOARD_MOTOR_FRONT_RIGHT];
        g_encoder_samples[BOARD_MOTOR_REAR_LEFT] = samples[BOARD_MOTOR_REAR_LEFT];
        g_encoder_samples[BOARD_MOTOR_REAR_RIGHT] = samples[BOARD_MOTOR_REAR_RIGHT];
        motor_control_step(samples);
        ++g_encoder_sample_sequence;

        board_motor_set_signed_duty(BOARD_MOTOR_FRONT_LEFT,
                                    motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT));
        board_motor_set_signed_duty(BOARD_MOTOR_FRONT_RIGHT,
                                    motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_RIGHT));
        board_motor_set_signed_duty(BOARD_MOTOR_REAR_LEFT,
                                    motor_control_get_output_duty_percent(BOARD_MOTOR_REAR_LEFT));
        board_motor_set_signed_duty(BOARD_MOTOR_REAR_RIGHT,
                                    motor_control_get_output_duty_percent(BOARD_MOTOR_REAR_RIGHT));
        vTaskDelayUntil(&last_wake_time, interval);
    }
}

static void ws2812_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(500U);
    board_ws2812_pixel_t pixels[BOARD_WS2812_PIXEL_COUNT] = {0};
    uint32_t pixel_index = 0U;
    uint32_t color_index = 0U;

    (void)argument;
    for (;;) {
        uint32_t index;

        for (index = 0U; index < BOARD_WS2812_PIXEL_COUNT; ++index) {
            pixels[index].red = 0U;
            pixels[index].green = 0U;
            pixels[index].blue = 0U;
        }

        if (color_index == 0U) {
            pixels[pixel_index].red = WS2812_BRIGHTNESS;
        } else if (color_index == 1U) {
            pixels[pixel_index].green = WS2812_BRIGHTNESS;
        } else if (color_index == 2U) {
            pixels[pixel_index].blue = WS2812_BRIGHTNESS;
        } else {
            pixels[pixel_index].red = WS2812_BRIGHTNESS;
            pixels[pixel_index].green = WS2812_BRIGHTNESS;
            pixels[pixel_index].blue = WS2812_BRIGHTNESS;
        }
        board_ws2812_write(pixels);

        ++color_index;
        if (color_index == 4U) {
            color_index = 0U;
            ++pixel_index;
            if (pixel_index == BOARD_WS2812_PIXEL_COUNT) {
                pixel_index = 0U;
            }
        }
        vTaskDelayUntil(&last_wake_time, interval);
    }
}

static void uart_echo_task(void *argument)
{
    QueueHandle_t queue = (QueueHandle_t)argument;
    uint8_t byte;
    for (;;) {
        if (xQueueReceive(queue, &byte, portMAX_DELAY) == pdPASS) {
            board_uart_write(&byte, 1U);
        }
    }
}

static void motor_control_snapshot_copy(motor_control_wheel_status_t control[BOARD_MOTOR_COUNT])
{
    uint32_t begin_sequence;
    uint32_t end_sequence;
    uint32_t wheel;

    for (;;) {
        begin_sequence = g_encoder_sample_sequence;
        if ((begin_sequence & 1U) == 0U) {
            for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
                control[wheel] = g_motor_control_status[wheel];
            }
            end_sequence = g_encoder_sample_sequence;
            if ((begin_sequence == end_sequence) && ((end_sequence & 1U) == 0U)) {
                break;
            }
        }
    }
}

#if ENCODER_TELEMETRY_ENABLE
static void telemetry_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(ENCODER_TELEMETRY_INTERVAL_MS);
    motor_control_wheel_status_t control[BOARD_MOTOR_COUNT];
    char message[512];

    (void)argument;
    for (;;) {
        int length;

        motor_control_snapshot_copy(control);
        length = snprintf(message, sizeof(message),
                          "ctl,fl=%ld,%ld,%ld,%ld,%ld,%ld,fr=%ld,%ld,%ld,%ld,%ld,%ld,rl=%ld,%ld,%ld,%ld,%ld,%ld,rr=%ld,%ld,%ld,%ld,%ld,%ld,dbg=%u,%u,%u,%ld,%ld,%ld,%ld\r\n",
                          (long)control[BOARD_MOTOR_FRONT_LEFT].target_speed_mm_per_s,
                          (long)control[BOARD_MOTOR_FRONT_LEFT].feedback_speed_mm_per_s,
                          (long)control[BOARD_MOTOR_FRONT_LEFT].pid_p_out,
                          (long)control[BOARD_MOTOR_FRONT_LEFT].pid_i_out,
                          (long)control[BOARD_MOTOR_FRONT_LEFT].pid_d_out,
                          (long)control[BOARD_MOTOR_FRONT_LEFT].output_duty_percent,
                          (long)control[BOARD_MOTOR_FRONT_RIGHT].target_speed_mm_per_s,
                          (long)control[BOARD_MOTOR_FRONT_RIGHT].feedback_speed_mm_per_s,
                          (long)control[BOARD_MOTOR_FRONT_RIGHT].pid_p_out,
                          (long)control[BOARD_MOTOR_FRONT_RIGHT].pid_i_out,
                          (long)control[BOARD_MOTOR_FRONT_RIGHT].pid_d_out,
                          (long)control[BOARD_MOTOR_FRONT_RIGHT].output_duty_percent,
                          (long)control[BOARD_MOTOR_REAR_LEFT].target_speed_mm_per_s,
                          (long)control[BOARD_MOTOR_REAR_LEFT].feedback_speed_mm_per_s,
                          (long)control[BOARD_MOTOR_REAR_LEFT].pid_p_out,
                          (long)control[BOARD_MOTOR_REAR_LEFT].pid_i_out,
                          (long)control[BOARD_MOTOR_REAR_LEFT].pid_d_out,
                          (long)control[BOARD_MOTOR_REAR_LEFT].output_duty_percent,
                          (long)control[BOARD_MOTOR_REAR_RIGHT].target_speed_mm_per_s,
                          (long)control[BOARD_MOTOR_REAR_RIGHT].feedback_speed_mm_per_s,
                          (long)control[BOARD_MOTOR_REAR_RIGHT].pid_p_out,
                          (long)control[BOARD_MOTOR_REAR_RIGHT].pid_i_out,
                          (long)control[BOARD_MOTOR_REAR_RIGHT].pid_d_out,
                          (long)control[BOARD_MOTOR_REAR_RIGHT].output_duty_percent,
                          (unsigned)g_motor_debug.enable,
                          (unsigned)g_motor_debug.mode,
                          (unsigned)g_motor_debug.wheel,
                          (long)g_motor_debug.target_duty_percent,
                          (long)g_motor_debug.target_speed_mm_per_s,
                          (long)g_motor_debug.feedback_speed_mm_per_s,
                          (long)g_motor_debug.output_duty_percent);
        if ((length > 0) && ((size_t)length < sizeof(message))) {
            board_uart_write((const uint8_t *)message, (size_t)length);
        }
        vTaskDelayUntil(&last_wake_time, interval);
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
#if IMU_TELEMETRY_ENABLE
    char message[128];
#endif

    (void)argument;
    for (;;) {
#if IMU_TELEMETRY_ENABLE
        int length;
#endif

        chip_id = 0U;
        status = board_bmi160_init(&chip_id);
#if IMU_TELEMETRY_ENABLE
        length = snprintf(message, sizeof(message),
                          "bmi160,id=0x%02X,status=%u\r\n",
                          chip_id, (unsigned)status);
        if ((length > 0) && ((size_t)length < sizeof(message))) {
            board_uart_write((const uint8_t *)message, (size_t)length);
        }
#endif
        if (status != BOARD_BMI160_STATUS_OK) {
            vTaskDelay(pdMS_TO_TICKS(1000U));
        } else {
            consecutive_failures = 0U;
            last_wake_time = xTaskGetTickCount();
            for (;;) {
                status = board_bmi160_read_sample(&sample);
                if (status != BOARD_BMI160_STATUS_OK) {
                    ++consecutive_failures;
#if IMU_TELEMETRY_ENABLE
                    length = snprintf(message, sizeof(message),
                                      "bmi160,error=%u\r\n", (unsigned)status);
                    if ((length > 0) && ((size_t)length < sizeof(message))) {
                        board_uart_write((const uint8_t *)message, (size_t)length);
                    }
#endif
                    if (consecutive_failures >= IMU_REINIT_FAILURE_THRESHOLD) {
                        break;
                    }
                } else {
                    consecutive_failures = 0U;
#if IMU_TELEMETRY_ENABLE
                    length = snprintf(message, sizeof(message),
                                      "imu,ax=%+6d,ay=%+6d,az=%+6d,gx=%+6d,gy=%+6d,gz=%+6d\r\n",
                                      sample.accel_x, sample.accel_y, sample.accel_z,
                                      sample.gyro_x, sample.gyro_y, sample.gyro_z);
                    if ((length > 0) && ((size_t)length < sizeof(message))) {
                        board_uart_write((const uint8_t *)message, (size_t)length);
                    }
#endif
                }
                vTaskDelayUntil(&last_wake_time, interval);
            }
        }
    }
}
#if BUZZER_FEATURE_ENABLE
static void buzzer_task(void *argument)
{
    (void)argument;
    for (;;) {
        board_buzzer_start();
        vTaskDelay(pdMS_TO_TICKS(BUZZER_ON_TIME_MS));
        board_buzzer_stop();
        vTaskDelay(pdMS_TO_TICKS(BUZZER_OFF_TIME_MS));
    }
}
#endif

void vApplicationMallocFailedHook(void) { taskDISABLE_INTERRUPTS(); for (;;) { } }
void vApplicationGetIdleTaskMemory(StaticTask_t **task_buffer, StackType_t **stack_buffer, uint32_t *stack_size)
{ *task_buffer = &g_idle_task_buffer; *stack_buffer = g_idle_task_stack; *stack_size = configIDLE_TASK_STACK_DEPTH; }
void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{ (void)task; (void)task_name; taskDISABLE_INTERRUPTS(); for (;;) { } }

int main(void)
{
    QueueHandle_t uart_queue;
    SYSCFG_DL_init();
    board_encoder_init();
    motor_control_init();
    NVIC_EnableIRQ(GPIOA_INT_IRQn);
    board_buzzer_init(BUZZER_FREQUENCY_HZ, BUZZER_DUTY_PERCENT);
    uart_queue = xQueueCreateStatic(UART_RX_QUEUE_LENGTH, sizeof(uint8_t), g_uart_queue_storage, &g_uart_queue_buffer);
    configASSERT(uart_queue != NULL);
    board_uart_enable_rx_interrupt(uart_queue);
    configASSERT(xTaskCreateStatic(motor_task, "motor", MOTOR_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_motor_task_stack, &g_motor_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(ws2812_task, "ws2812", WS2812_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_ws2812_task_stack, &g_ws2812_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(board_uart_tx_task, "uart_tx", UART_TX_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_uart_tx_task_stack, &g_uart_tx_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(uart_echo_task, "uart", UART_TASK_STACK_DEPTH, uart_queue, APP_TASK_PRIORITY, g_uart_task_stack, &g_uart_task_buffer) != NULL);
#if ENCODER_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(telemetry_task, "telemetry", ENCODER_TELEMETRY_TASK_STACK_DEPTH, NULL, TELEMETRY_TASK_PRIORITY, g_telemetry_task_stack, &g_telemetry_task_buffer) != NULL);
#endif
    configASSERT(xTaskCreateStatic(imu_task, "imu", IMU_TASK_STACK_DEPTH, NULL, IMU_TASK_PRIORITY, g_imu_task_stack, &g_imu_task_buffer) != NULL);
#if BUZZER_FEATURE_ENABLE
    configASSERT(xTaskCreateStatic(buzzer_task, "buzzer", BUZZER_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_buzzer_task_stack, &g_buzzer_task_buffer) != NULL);
#endif
    vTaskStartScheduler();
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}
