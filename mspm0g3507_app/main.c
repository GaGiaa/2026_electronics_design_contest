#include <stdint.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "board_uart.h"
#include "board_ws2812.h"
#include "ti_msp_dl_config.h"

#define APP_TASK_PRIORITY 1U
#define WS2812_TASK_STACK_DEPTH 256U
#define UART_TASK_STACK_DEPTH 256U
#define UART_RX_QUEUE_LENGTH 64U
#define WS2812_BRIGHTNESS 16U

static StaticTask_t g_ws2812_task_buffer;
static StackType_t g_ws2812_task_stack[WS2812_TASK_STACK_DEPTH];
static StaticTask_t g_uart_task_buffer;
static StackType_t g_uart_task_stack[UART_TASK_STACK_DEPTH];
static StaticQueue_t g_uart_queue_buffer;
static uint8_t g_uart_queue_storage[UART_RX_QUEUE_LENGTH * sizeof(uint8_t)];
static StaticTask_t g_idle_task_buffer;
static StackType_t g_idle_task_stack[configIDLE_TASK_STACK_DEPTH];

void UART_0_INST_IRQHandler(void) { board_uart_irq_handler(); }

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

void vApplicationMallocFailedHook(void) { taskDISABLE_INTERRUPTS(); for (;;) { } }
void vApplicationGetIdleTaskMemory(StaticTask_t **task_buffer, StackType_t **stack_buffer, uint32_t *stack_size)
{ *task_buffer = &g_idle_task_buffer; *stack_buffer = g_idle_task_stack; *stack_size = configIDLE_TASK_STACK_DEPTH; }
void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{ (void)task; (void)task_name; taskDISABLE_INTERRUPTS(); for (;;) { } }

int main(void)
{
    QueueHandle_t uart_queue;
    SYSCFG_DL_init();
    uart_queue = xQueueCreateStatic(UART_RX_QUEUE_LENGTH, sizeof(uint8_t), g_uart_queue_storage, &g_uart_queue_buffer);
    configASSERT(uart_queue != NULL);
    board_uart_enable_rx_interrupt(uart_queue);
    configASSERT(xTaskCreateStatic(ws2812_task, "ws2812", WS2812_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_ws2812_task_stack, &g_ws2812_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(uart_echo_task, "uart", UART_TASK_STACK_DEPTH, uart_queue, APP_TASK_PRIORITY, g_uart_task_stack, &g_uart_task_buffer) != NULL);
    vTaskStartScheduler();
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}
