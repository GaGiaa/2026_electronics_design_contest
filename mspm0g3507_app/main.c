#include <stdint.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "board_buzzer.h"
#include "board_motor.h"
#include "board_uart.h"
#include "board_ws2812.h"
#include "ti_msp_dl_config.h"

#define APP_TASK_PRIORITY 1U
#define MOTOR_TASK_STACK_DEPTH 256U
#define WS2812_TASK_STACK_DEPTH 256U
#define UART_TASK_STACK_DEPTH 256U
#define BUZZER_TASK_STACK_DEPTH 128U
#define UART_RX_QUEUE_LENGTH 64U
#define WS2812_BRIGHTNESS 16U

#define BUZZER_FEATURE_ENABLE 0U
#define BUZZER_FREQUENCY_HZ 2000U
#define BUZZER_DUTY_PERCENT 10U
#define BUZZER_ON_TIME_MS 200U
#define BUZZER_OFF_TIME_MS 1800U

#if (BUZZER_FREQUENCY_HZ < 1000U) || (BUZZER_FREQUENCY_HZ > 20000U)
#error "BUZZER_FREQUENCY_HZ must be between 1000 Hz and 20000 Hz"
#endif

#if (BUZZER_ON_TIME_MS == 0U) || (BUZZER_OFF_TIME_MS == 0U)
#error "BUZZER_ON_TIME_MS and BUZZER_OFF_TIME_MS must be nonzero"
#endif

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
#if BUZZER_FEATURE_ENABLE
static StaticTask_t g_buzzer_task_buffer;
static StackType_t g_buzzer_task_stack[BUZZER_TASK_STACK_DEPTH];
#endif
static StaticQueue_t g_uart_queue_buffer;
static uint8_t g_uart_queue_storage[UART_RX_QUEUE_LENGTH * sizeof(uint8_t)];
static StaticTask_t g_idle_task_buffer;
static StackType_t g_idle_task_stack[configIDLE_TASK_STACK_DEPTH];

void UART_0_INST_IRQHandler(void) { board_uart_irq_handler(); }

static void motor_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(10U);

    (void)argument;
    for (;;) {
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
            while (board_uart_tx_full()) { taskYIELD(); }
            board_uart_transmit(byte);
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
    board_buzzer_init(BUZZER_FREQUENCY_HZ, BUZZER_DUTY_PERCENT);
    uart_queue = xQueueCreateStatic(UART_RX_QUEUE_LENGTH, sizeof(uint8_t), g_uart_queue_storage, &g_uart_queue_buffer);
    configASSERT(uart_queue != NULL);
    board_uart_enable_rx_interrupt(uart_queue);
    configASSERT(xTaskCreateStatic(motor_task, "motor", MOTOR_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_motor_task_stack, &g_motor_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(ws2812_task, "ws2812", WS2812_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_ws2812_task_stack, &g_ws2812_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(uart_echo_task, "uart", UART_TASK_STACK_DEPTH, uart_queue, APP_TASK_PRIORITY, g_uart_task_stack, &g_uart_task_buffer) != NULL);
#if BUZZER_FEATURE_ENABLE
    configASSERT(xTaskCreateStatic(buzzer_task, "buzzer", BUZZER_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_buzzer_task_stack, &g_buzzer_task_buffer) != NULL);
#endif
    vTaskStartScheduler();
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}
