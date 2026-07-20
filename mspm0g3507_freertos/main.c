#include <stdint.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "board_led.h"
#include "board_uart.h"
#include "ti_msp_dl_config.h"

#ifndef LED_TOGGLE_INTERVAL_MS
#define LED_TOGGLE_INTERVAL_MS 2000U
#endif

#if (LED_TOGGLE_INTERVAL_MS == 0U)
#error "LED_TOGGLE_INTERVAL_MS must be greater than zero."
#endif

#define APP_TASK_PRIORITY       1U
#define LED_TASK_STACK_DEPTH    192U
#define UART_TASK_STACK_DEPTH   256U
#define UART_RX_QUEUE_LENGTH    64U

static StaticTask_t g_led_task_buffer;
static StackType_t g_led_task_stack[LED_TASK_STACK_DEPTH];
static StaticTask_t g_uart_task_buffer;
static StackType_t g_uart_task_stack[UART_TASK_STACK_DEPTH];
static StaticQueue_t g_uart_queue_buffer;
static uint8_t g_uart_queue_storage[UART_RX_QUEUE_LENGTH * sizeof(uint8_t)];
static StaticTask_t g_idle_task_buffer;
static StackType_t g_idle_task_stack[configIDLE_TASK_STACK_DEPTH];

void UART_0_INST_IRQHandler(void)
{
    board_uart_irq_handler();
}

static void led_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(LED_TOGGLE_INTERVAL_MS);

    (void) argument;

    for (;;) {
        vTaskDelayUntil(&last_wake_time, interval);
        board_led_toggle();
    }
}

static void uart_echo_task(void *argument)
{
    QueueHandle_t queue = (QueueHandle_t) argument;
    uint8_t byte;

    for (;;) {
        if (xQueueReceive(queue, &byte, portMAX_DELAY) == pdPASS) {
            while (board_uart_tx_full()) {
                taskYIELD();
            }
            board_uart_transmit(byte);
        }
    }
}

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vApplicationGetIdleTaskMemory(StaticTask_t **task_buffer,
                                   StackType_t **stack_buffer,
                                   uint32_t *stack_size)
{
    *task_buffer = &g_idle_task_buffer;
    *stack_buffer = g_idle_task_stack;
    *stack_size = configIDLE_TASK_STACK_DEPTH;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void) task;
    (void) task_name;
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

int main(void)
{
    QueueHandle_t uart_queue;

    SYSCFG_DL_init();

    uart_queue = xQueueCreateStatic(UART_RX_QUEUE_LENGTH, sizeof(uint8_t),
                                    g_uart_queue_storage, &g_uart_queue_buffer);
    configASSERT(uart_queue != NULL);
    board_uart_enable_rx_interrupt(uart_queue);

    configASSERT(xTaskCreateStatic(led_task, "led", LED_TASK_STACK_DEPTH, NULL,
                                   APP_TASK_PRIORITY, g_led_task_stack,
                                   &g_led_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(uart_echo_task, "uart", UART_TASK_STACK_DEPTH,
                                   uart_queue, APP_TASK_PRIORITY, g_uart_task_stack,
                                   &g_uart_task_buffer) != NULL);

    vTaskStartScheduler();

    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}
