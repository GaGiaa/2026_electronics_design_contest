#include <stdint.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "board_encoder.h"
#include "board_bmi160.h"
#include "board_imu_yaw.h"
#include "board_buttons.h"
#include "board_buzzer.h"
#include "board_grayscale.h"
#include "board_motor.h"
#include "board_uart.h"
#include "board_ws2812.h"
#include "motor_control.h"
#include "ti_msp_dl_config.h"
#include "vofa_justfloat.h"

#define APP_TASK_PRIORITY 1U
#define MOTOR_TASK_STACK_DEPTH 256U
#define WS2812_TASK_STACK_DEPTH 256U
#define UART_TASK_STACK_DEPTH 256U
#define UART_TX_TASK_STACK_DEPTH 256U
#define BUZZER_TASK_STACK_DEPTH 128U
#define BUTTON_TASK_STACK_DEPTH 128U
#define BUTTON_FEATURE_ENABLE 0U
#define UART_RX_QUEUE_LENGTH 64U
#define WS2812_BRIGHTNESS 16U
#define BUTTON_TASK_INTERVAL_MS 10U
#define TELEMETRY_TASK_PRIORITY 0U
#ifndef VOFA_SPEED_PID_TELEMETRY_ENABLE
#define VOFA_SPEED_PID_TELEMETRY_ENABLE 0U
#endif
#define VOFA_SPEED_PID_TELEMETRY_INTERVAL_MS 10U
#define VOFA_SPEED_PID_TELEMETRY_TASK_STACK_DEPTH 256U
#define VOFA_SPEED_PID_TELEMETRY_TASK_PRIORITY 0U
#ifndef IMU_TELEMETRY_ENABLE
#define IMU_TELEMETRY_ENABLE 0U
#endif
#ifndef IMU_YAW_ENABLE
#define IMU_YAW_ENABLE 0U
#endif
#define IMU_YAW_TRACK_WIDTH_MM 130.0f
#define IMU_TASK_STACK_DEPTH 512U
#define IMU_TASK_PRIORITY 0U
#define IMU_SAMPLE_INTERVAL_MS 10U
#define IMU_REINIT_FAILURE_THRESHOLD 3U

#if VOFA_SPEED_PID_TELEMETRY_ENABLE && IMU_TELEMETRY_ENABLE
#error "IMU telemetry and VOFA telemetry cannot be enabled together"
#endif
#if IMU_TELEMETRY_ENABLE && !IMU_YAW_ENABLE
#error "IMU telemetry requires IMU yaw to be enabled"
#endif

#define GRAY_TELEMETRY_ENABLE 0U
#define GRAY_SAMPLE_INTERVAL_MS 10U
#define GRAY_TELEMETRY_INTERVAL_MS 100U
#define GRAY_TASK_STACK_DEPTH 512U

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
#if !VOFA_SPEED_PID_TELEMETRY_ENABLE
static StaticTask_t g_uart_task_buffer;
static StackType_t g_uart_task_stack[UART_TASK_STACK_DEPTH];
#endif
static StaticTask_t g_uart_tx_task_buffer;
static StackType_t g_uart_tx_task_stack[UART_TX_TASK_STACK_DEPTH];
#if BUTTON_FEATURE_ENABLE
static StaticTask_t g_button_task_buffer;
static StackType_t g_button_task_stack[BUTTON_TASK_STACK_DEPTH];
#endif
#if VOFA_SPEED_PID_TELEMETRY_ENABLE
static StaticTask_t g_telemetry_task_buffer;
static StackType_t g_telemetry_task_stack[VOFA_SPEED_PID_TELEMETRY_TASK_STACK_DEPTH];
#endif
static StaticTask_t g_imu_task_buffer;
static StackType_t g_imu_task_stack[IMU_TASK_STACK_DEPTH];
static StaticTask_t g_gray_task_buffer;
static StackType_t g_gray_task_stack[GRAY_TASK_STACK_DEPTH];
#if GRAY_TELEMETRY_ENABLE
static StaticTask_t g_gray_telemetry_task_buffer;
static StackType_t g_gray_telemetry_task_stack[GRAY_TASK_STACK_DEPTH];
#endif
#if BUZZER_FEATURE_ENABLE
static StaticTask_t g_buzzer_task_buffer;
static StackType_t g_buzzer_task_stack[BUZZER_TASK_STACK_DEPTH];
#endif
static StaticQueue_t g_uart_queue_buffer;
static uint8_t g_uart_queue_storage[UART_RX_QUEUE_LENGTH * sizeof(uint8_t)];
static StaticTask_t g_idle_task_buffer;
static StackType_t g_idle_task_stack[configIDLE_TASK_STACK_DEPTH];
volatile board_encoder_sample_t g_encoder_samples[BOARD_MOTOR_COUNT];
volatile board_grayscale_snapshot_t g_grayscale_snapshot;
static volatile uint32_t g_grayscale_publish_sequence;
static volatile uint32_t g_encoder_sample_sequence;

#if BUTTON_FEATURE_ENABLE
typedef struct {
    const uint8_t *data;
    size_t length;
} button_message_t;

#define BUTTON_MESSAGE(text) { (const uint8_t *)(text), sizeof(text) - 1U }

static const button_message_t g_button_down_messages[BOARD_BUTTON_COUNT] = {
    BUTTON_MESSAGE("key,pa7=down\r\n"),
    BUTTON_MESSAGE("key,pb12=down\r\n"),
    BUTTON_MESSAGE("key,pa8=down\r\n"),
    BUTTON_MESSAGE("key,pa30=down\r\n")
};
static const button_message_t g_button_up_messages[BOARD_BUTTON_COUNT] = {
    BUTTON_MESSAGE("key,pa7=up\r\n"),
    BUTTON_MESSAGE("key,pb12=up\r\n"),
    BUTTON_MESSAGE("key,pa8=up\r\n"),
    BUTTON_MESSAGE("key,pa30=up\r\n")
};

#undef BUTTON_MESSAGE
#endif

void UART_0_INST_IRQHandler(void) { board_uart_irq_handler(); }
void GROUP1_IRQHandler(void)
{
    switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
    case ENCODER_GPIOA_INT_IIDX:
        board_encoder_gpioa_irq_handler();
        break;
    case ENCODER_GPIOB_INT_IIDX:
        board_encoder_gpiob_irq_handler();
        break;
    default:
        break;
    }
}

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

#if BUTTON_FEATURE_ENABLE
static void button_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(BUTTON_TASK_INTERVAL_MS);

    (void)argument;
    for (;;) {
        board_buttons_events_t events = board_buttons_scan();
        uint32_t button;

        for (button = 0U; button < BOARD_BUTTON_COUNT; ++button) {
            uint32_t mask = 1U << button;
            if ((events.pressed_mask & mask) != 0U) {
                board_uart_write(g_button_down_messages[button].data,
                                 g_button_down_messages[button].length);
            }
            if ((events.released_mask & mask) != 0U) {
                board_uart_write(g_button_up_messages[button].data,
                                 g_button_up_messages[button].length);
            }
        }
        vTaskDelayUntil(&last_wake_time, interval);
    }
}
#endif

#if !VOFA_SPEED_PID_TELEMETRY_ENABLE
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
#endif

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

static void encoder_samples_snapshot_copy(board_encoder_sample_t samples[BOARD_MOTOR_COUNT])
{
    uint32_t begin_sequence;
    uint32_t end_sequence;
    uint32_t wheel;

    for (;;) {
        begin_sequence = g_encoder_sample_sequence;
        if ((begin_sequence & 1U) == 0U) {
            for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
                samples[wheel] = g_encoder_samples[wheel];
            }
            end_sequence = g_encoder_sample_sequence;
            if ((begin_sequence == end_sequence) && ((end_sequence & 1U) == 0U)) {
                break;
            }
        }
    }
}

#if VOFA_SPEED_PID_TELEMETRY_ENABLE
static void telemetry_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(VOFA_SPEED_PID_TELEMETRY_INTERVAL_MS);
    motor_control_wheel_status_t control[BOARD_MOTOR_COUNT];
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE];

    (void)argument;
    for (;;) {
        board_motor_wheel_t wheel = g_motor_debug.wheel;

        motor_control_snapshot_copy(control);
        if (wheel >= BOARD_MOTOR_COUNT) {
            wheel = BOARD_MOTOR_FRONT_LEFT;
        }
        if (vofa_justfloat_encode3(frame, sizeof(frame),
                                   control[wheel].target_speed_mm_per_s,
                                   control[wheel].feedback_speed_mm_per_s,
                                   control[wheel].output_duty_percent)) {
            board_uart_write(frame, sizeof(frame));
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
#if IMU_YAW_ENABLE
    board_imu_yaw_state_t yaw_state;
    board_encoder_sample_t encoder_samples[BOARD_MOTOR_COUNT];
#endif
#if IMU_TELEMETRY_ENABLE
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE];
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
                    encoder_samples_snapshot_copy(encoder_samples);
                    board_imu_yaw_update(&yaw_state, &sample, encoder_samples,
                                         (float)IMU_SAMPLE_INTERVAL_MS / 1000.0f);
#endif
#if IMU_YAW_ENABLE
                    if (vofa_justfloat_encode3(frame, sizeof(frame), yaw_state.yaw_deg, yaw_state.yaw_rate_dps, yaw_state.gyro_bias_z_dps)) {
                        board_uart_write(frame, sizeof(frame));
                    }
#endif
                }
                vTaskDelayUntil(&last_wake_time, interval);
            }
        }
    }
}

static void grayscale_snapshot_copy(board_grayscale_snapshot_t *snapshot)
{
    uint32_t begin_sequence;
    uint32_t end_sequence;
    uint32_t channel;

    for (;;) {
        begin_sequence = g_grayscale_publish_sequence;
        if ((begin_sequence & 1U) == 0U) {
            for (channel = 0U; channel < BOARD_GRAYSCALE_CHANNEL_COUNT; ++channel) {
                snapshot->raw[channel] = g_grayscale_snapshot.raw[channel];
                snapshot->normalized[channel] = g_grayscale_snapshot.normalized[channel];
            }
            snapshot->digital = g_grayscale_snapshot.digital;
            snapshot->sequence = g_grayscale_snapshot.sequence;
            end_sequence = g_grayscale_publish_sequence;
            if ((begin_sequence == end_sequence) && ((end_sequence & 1U) == 0U)) {
                break;
            }
        }
    }
}

static void gray_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(GRAY_SAMPLE_INTERVAL_MS);
    board_grayscale_snapshot_t sample;
    uint32_t channel;

    (void)argument;
    for (;;) {
        board_grayscale_sample(&sample);
        ++g_grayscale_publish_sequence;
        for (channel = 0U; channel < BOARD_GRAYSCALE_CHANNEL_COUNT; ++channel) {
            g_grayscale_snapshot.raw[channel] = sample.raw[channel];
            g_grayscale_snapshot.normalized[channel] = sample.normalized[channel];
        }
        g_grayscale_snapshot.digital = sample.digital;
        g_grayscale_snapshot.sequence = sample.sequence;
        ++g_grayscale_publish_sequence;
        vTaskDelayUntil(&last_wake_time, interval);
    }
}

#if GRAY_TELEMETRY_ENABLE
static void gray_telemetry_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(GRAY_TELEMETRY_INTERVAL_MS);
    board_grayscale_snapshot_t snapshot;
    char message[256];

    (void)argument;
    for (;;) {
        int length;

        grayscale_snapshot_copy(&snapshot);
        length = snprintf(message, sizeof(message),
                          "gray,raw=%u,%u,%u,%u,%u,%u,%u,%u,norm=%u,%u,%u,%u,%u,%u,%u,%u,digital=0x%02X\r\n",
                          snapshot.raw[0], snapshot.raw[1], snapshot.raw[2], snapshot.raw[3],
                          snapshot.raw[4], snapshot.raw[5], snapshot.raw[6], snapshot.raw[7],
                          snapshot.normalized[0], snapshot.normalized[1], snapshot.normalized[2],
                          snapshot.normalized[3], snapshot.normalized[4], snapshot.normalized[5],
                          snapshot.normalized[6], snapshot.normalized[7], snapshot.digital);
        if ((length > 0) && ((size_t)length < sizeof(message))) {
            board_uart_write((const uint8_t *)message, (size_t)length);
        }
        vTaskDelayUntil(&last_wake_time, interval);
    }
}
#endif
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
    static const uint16_t grayscale_white[BOARD_GRAYSCALE_CHANNEL_COUNT] =
        {3000U, 3000U, 3000U, 3000U, 3000U, 3000U, 3000U, 3000U};
    static const uint16_t grayscale_black[BOARD_GRAYSCALE_CHANNEL_COUNT] =
        {500U, 500U, 500U, 500U, 500U, 500U, 500U, 500U};

    SYSCFG_DL_init();
#if BUTTON_FEATURE_ENABLE
    board_buttons_init();
#endif
    board_encoder_init();
    board_grayscale_init(grayscale_white, grayscale_black);
    motor_control_init();
    NVIC_EnableIRQ(GPIOA_INT_IRQn);
    board_buzzer_init(BUZZER_FREQUENCY_HZ, BUZZER_DUTY_PERCENT);
    uart_queue = xQueueCreateStatic(UART_RX_QUEUE_LENGTH, sizeof(uint8_t), g_uart_queue_storage, &g_uart_queue_buffer);
    configASSERT(uart_queue != NULL);
    board_uart_enable_rx_interrupt(uart_queue);
    configASSERT(xTaskCreateStatic(motor_task, "motor", MOTOR_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_motor_task_stack, &g_motor_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(ws2812_task, "ws2812", WS2812_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_ws2812_task_stack, &g_ws2812_task_buffer) != NULL);
#if BUTTON_FEATURE_ENABLE
    configASSERT(xTaskCreateStatic(button_task, "buttons", BUTTON_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_button_task_stack, &g_button_task_buffer) != NULL);
#endif
    configASSERT(xTaskCreateStatic(board_uart_tx_task, "uart_tx", UART_TX_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_uart_tx_task_stack, &g_uart_tx_task_buffer) != NULL);
#if !VOFA_SPEED_PID_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(uart_echo_task, "uart", UART_TASK_STACK_DEPTH, uart_queue, APP_TASK_PRIORITY, g_uart_task_stack, &g_uart_task_buffer) != NULL);
#endif
#if VOFA_SPEED_PID_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(telemetry_task, "telemetry", VOFA_SPEED_PID_TELEMETRY_TASK_STACK_DEPTH, NULL, VOFA_SPEED_PID_TELEMETRY_TASK_PRIORITY, g_telemetry_task_stack, &g_telemetry_task_buffer) != NULL);
#endif
    configASSERT(xTaskCreateStatic(imu_task, "imu", IMU_TASK_STACK_DEPTH, NULL, IMU_TASK_PRIORITY, g_imu_task_stack, &g_imu_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(gray_task, "gray", GRAY_TASK_STACK_DEPTH, NULL, TELEMETRY_TASK_PRIORITY, g_gray_task_stack, &g_gray_task_buffer) != NULL);
#if GRAY_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(gray_telemetry_task, "gray_tele", GRAY_TASK_STACK_DEPTH, NULL, TELEMETRY_TASK_PRIORITY, g_gray_telemetry_task_stack, &g_gray_telemetry_task_buffer) != NULL);
#endif
#if BUZZER_FEATURE_ENABLE
    configASSERT(xTaskCreateStatic(buzzer_task, "buzzer", BUZZER_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_buzzer_task_stack, &g_buzzer_task_buffer) != NULL);
#endif
    vTaskStartScheduler();
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}
