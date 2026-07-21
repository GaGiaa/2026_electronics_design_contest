#include <stdint.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "board_encoder.h"
#include "board_motor.h"
#include "board_uart.h"
#include "board_ws2812.h"
#include "ti_msp_dl_config.h"

#define APP_TASK_PRIORITY 1U
#define MOTOR_TASK_STACK_DEPTH 256U
#define WS2812_TASK_STACK_DEPTH 256U
#define UART_TASK_STACK_DEPTH 256U
#define UART_TX_TASK_STACK_DEPTH 256U
#define UART_RX_QUEUE_LENGTH 64U
#define WS2812_BRIGHTNESS 16U
#define ENCODER_TELEMETRY_INTERVAL_MS 100U
#define ENCODER_TELEMETRY_TASK_STACK_DEPTH 512U
#define TELEMETRY_TASK_PRIORITY 0U

#define MOTOR_FRONT_LEFT_DIRECTION BOARD_MOTOR_DIRECTION_FORWARD
#define MOTOR_FRONT_LEFT_DUTY_PERCENT 0U
#define MOTOR_FRONT_RIGHT_DIRECTION BOARD_MOTOR_DIRECTION_FORWARD
#define MOTOR_FRONT_RIGHT_DUTY_PERCENT 0U
#define MOTOR_REAR_LEFT_DIRECTION BOARD_MOTOR_DIRECTION_FORWARD
#define MOTOR_REAR_LEFT_DUTY_PERCENT 0U
#define MOTOR_REAR_RIGHT_DIRECTION BOARD_MOTOR_DIRECTION_FORWARD
#define MOTOR_REAR_RIGHT_DUTY_PERCENT 0U

static StaticTask_t g_motor_task_buffer;
static StackType_t g_motor_task_stack[MOTOR_TASK_STACK_DEPTH];
static StaticTask_t g_ws2812_task_buffer;
static StackType_t g_ws2812_task_stack[WS2812_TASK_STACK_DEPTH];
static StaticTask_t g_uart_task_buffer;
static StackType_t g_uart_task_stack[UART_TASK_STACK_DEPTH];
static StaticTask_t g_uart_tx_task_buffer;
static StackType_t g_uart_tx_task_stack[UART_TX_TASK_STACK_DEPTH];
static StaticTask_t g_telemetry_task_buffer;
static StackType_t g_telemetry_task_stack[ENCODER_TELEMETRY_TASK_STACK_DEPTH];
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

    (void)argument;
    for (;;) {
        ++g_encoder_sample_sequence;
        g_encoder_samples[BOARD_MOTOR_FRONT_LEFT] =
            board_encoder_sample(BOARD_MOTOR_FRONT_LEFT);
        g_encoder_samples[BOARD_MOTOR_FRONT_RIGHT] =
            board_encoder_sample(BOARD_MOTOR_FRONT_RIGHT);
        g_encoder_samples[BOARD_MOTOR_REAR_LEFT] =
            board_encoder_sample(BOARD_MOTOR_REAR_LEFT);
        g_encoder_samples[BOARD_MOTOR_REAR_RIGHT] =
            board_encoder_sample(BOARD_MOTOR_REAR_RIGHT);
        ++g_encoder_sample_sequence;

        board_motor_set(BOARD_MOTOR_FRONT_LEFT, MOTOR_FRONT_LEFT_DIRECTION,
                        MOTOR_FRONT_LEFT_DUTY_PERCENT);
        board_motor_set(BOARD_MOTOR_FRONT_RIGHT, MOTOR_FRONT_RIGHT_DIRECTION,
                        MOTOR_FRONT_RIGHT_DUTY_PERCENT);
        board_motor_set(BOARD_MOTOR_REAR_LEFT, MOTOR_REAR_LEFT_DIRECTION,
                        MOTOR_REAR_LEFT_DUTY_PERCENT);
        board_motor_set(BOARD_MOTOR_REAR_RIGHT, MOTOR_REAR_RIGHT_DIRECTION,
                        MOTOR_REAR_RIGHT_DUTY_PERCENT);
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

static int32_t speed_as_mm_per_s(float speed)
{
    return (int32_t)speed;
}

static void encoder_samples_copy(board_encoder_sample_t samples[BOARD_MOTOR_COUNT])
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

static void telemetry_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(ENCODER_TELEMETRY_INTERVAL_MS);
    board_encoder_sample_t samples[BOARD_MOTOR_COUNT];
    char message[192];

    (void)argument;
    for (;;) {
        int length;

        encoder_samples_copy(samples);
        length = snprintf(message, sizeof(message),
                          "enc,fl=%ld,%ld,%ld,fr=%ld,%ld,%ld,rl=%ld,%ld,%ld,rr=%ld,%ld,%ld\r\n",
                          (long)samples[BOARD_MOTOR_FRONT_LEFT].delta_counts,
                          (long)samples[BOARD_MOTOR_FRONT_LEFT].total_counts,
                          (long)speed_as_mm_per_s(samples[BOARD_MOTOR_FRONT_LEFT].speed_mm_per_s),
                          (long)samples[BOARD_MOTOR_FRONT_RIGHT].delta_counts,
                          (long)samples[BOARD_MOTOR_FRONT_RIGHT].total_counts,
                          (long)speed_as_mm_per_s(samples[BOARD_MOTOR_FRONT_RIGHT].speed_mm_per_s),
                          (long)samples[BOARD_MOTOR_REAR_LEFT].delta_counts,
                          (long)samples[BOARD_MOTOR_REAR_LEFT].total_counts,
                          (long)speed_as_mm_per_s(samples[BOARD_MOTOR_REAR_LEFT].speed_mm_per_s),
                          (long)samples[BOARD_MOTOR_REAR_RIGHT].delta_counts,
                          (long)samples[BOARD_MOTOR_REAR_RIGHT].total_counts,
                          (long)speed_as_mm_per_s(samples[BOARD_MOTOR_REAR_RIGHT].speed_mm_per_s));
        if ((length > 0) && ((size_t)length < sizeof(message))) {
            board_uart_write((const uint8_t *)message, (size_t)length);
        }
        vTaskDelayUntil(&last_wake_time, interval);
    }
}

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
    NVIC_EnableIRQ(GPIOA_INT_IRQn);
    uart_queue = xQueueCreateStatic(UART_RX_QUEUE_LENGTH, sizeof(uint8_t), g_uart_queue_storage, &g_uart_queue_buffer);
    configASSERT(uart_queue != NULL);
    board_uart_enable_rx_interrupt(uart_queue);
    configASSERT(xTaskCreateStatic(motor_task, "motor", MOTOR_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_motor_task_stack, &g_motor_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(ws2812_task, "ws2812", WS2812_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_ws2812_task_stack, &g_ws2812_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(board_uart_tx_task, "uart_tx", UART_TX_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_uart_tx_task_stack, &g_uart_tx_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(uart_echo_task, "uart", UART_TASK_STACK_DEPTH, uart_queue, APP_TASK_PRIORITY, g_uart_task_stack, &g_uart_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(telemetry_task, "telemetry", ENCODER_TELEMETRY_TASK_STACK_DEPTH, NULL, TELEMETRY_TASK_PRIORITY, g_telemetry_task_stack, &g_telemetry_task_buffer) != NULL);
    vTaskStartScheduler();
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}
