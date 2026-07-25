#include "app/app_tasks_io.h"

#include <stdint.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "app/app_profile.h"
#include "drivers/buttons/board_buttons.h"
#include "drivers/buzzer/board_buzzer.h"
#include "drivers/oled/board_oled.h"
#include "drivers/servo/board_servo.h"
#include "drivers/uart/board_uart.h"
#include "drivers/ws2812/board_ws2812.h"
#include "app/app_state.h"

static StaticTask_t g_ws2812_task_buffer;
static StackType_t g_ws2812_task_stack[WS2812_TASK_STACK_DEPTH];
#if OLED_TEST_TASK_ENABLE
static StaticTask_t g_oled_test_task_buffer;
static StackType_t g_oled_test_task_stack[OLED_TEST_TASK_STACK_DEPTH];
#endif
#if !VOFA_SPEED_PID_TELEMETRY_ENABLE && !GRAY_VOFA_TELEMETRY_ENABLE && !IMU_TELEMETRY_ENABLE
static StaticTask_t g_uart_task_buffer;
static StackType_t g_uart_task_stack[UART_TASK_STACK_DEPTH];
#endif
static StaticTask_t g_uart_tx_task_buffer;
static StackType_t g_uart_tx_task_stack[UART_TX_TASK_STACK_DEPTH];
#if BUTTON_FEATURE_ENABLE
static StaticTask_t g_button_task_buffer;
static StackType_t g_button_task_stack[BUTTON_TASK_STACK_DEPTH];
#endif
static StaticQueue_t g_uart_queue_buffer;
static uint8_t g_uart_queue_storage[UART_RX_QUEUE_LENGTH * sizeof(uint8_t)];

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

#if OLED_TEST_TASK_ENABLE
static void oled_test_task(void *argument)
{
    uint32_t update_count = 0U;
    board_oled_status_t status;

    (void)argument;
    for (;;) {
        status = board_oled_init();
        if (status == BOARD_OLED_STATUS_OK) {
            for (;;) {
                char counter_text[12];

                board_oled_clear();
                board_oled_set_cursor(0U, 0U);
                board_oled_write_string("OLED TEST");
                board_oled_set_cursor(0U, 2U);
                board_oled_write_string("MSPM0G3507");
                board_oled_set_cursor(0U, 4U);
                board_oled_write_string("I2C0 PA0/PA1");
                board_oled_set_cursor(0U, 6U);
                board_oled_write_string("COUNT:");
                (void)snprintf(counter_text, sizeof(counter_text), "%05lu",
                               (unsigned long)update_count++);
                board_oled_write_string(counter_text);
                status = board_oled_update();
                if (status != BOARD_OLED_STATUS_OK) {
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(1000U));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}
#endif

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

#if !VOFA_SPEED_PID_TELEMETRY_ENABLE && !GRAY_VOFA_TELEMETRY_ENABLE && !IMU_TELEMETRY_ENABLE
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

#if BUZZER_FEATURE_ENABLE
static StaticTask_t g_buzzer_task_buffer;
static StackType_t g_buzzer_task_stack[BUZZER_TASK_STACK_DEPTH];

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

#if SERVO_FEATURE_ENABLE
static StaticTask_t g_servo_task_buffer;
static StackType_t g_servo_task_stack[SERVO_TASK_STACK_DEPTH];

static void servo_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();

    (void)argument;
    for (;;) {
        g_servo_pulse_us = board_servo_set_angle_deg(g_servo_angle_deg);
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(SERVO_TASK_INTERVAL_MS));
    }
}
#endif

void app_tasks_io_start(void)
{
    QueueHandle_t uart_queue;

    uart_queue = xQueueCreateStatic(UART_RX_QUEUE_LENGTH, sizeof(uint8_t),
                                     g_uart_queue_storage, &g_uart_queue_buffer);
    configASSERT(uart_queue != NULL);
    board_uart_enable_rx_interrupt(uart_queue);

    configASSERT(xTaskCreateStatic(ws2812_task, "ws2812",
                                   WS2812_TASK_STACK_DEPTH, NULL,
                                   APP_TASK_PRIORITY, g_ws2812_task_stack,
                                   &g_ws2812_task_buffer) != NULL);
#if OLED_TEST_TASK_ENABLE
    configASSERT(xTaskCreateStatic(oled_test_task, "oled",
                                   OLED_TEST_TASK_STACK_DEPTH, NULL,
                                   APP_TASK_PRIORITY, g_oled_test_task_stack,
                                   &g_oled_test_task_buffer) != NULL);
#endif
#if BUTTON_FEATURE_ENABLE
    configASSERT(xTaskCreateStatic(button_task, "buttons",
                                   BUTTON_TASK_STACK_DEPTH, NULL,
                                   APP_TASK_PRIORITY, g_button_task_stack,
                                   &g_button_task_buffer) != NULL);
#endif
    configASSERT(xTaskCreateStatic(board_uart_tx_task, "uart_tx",
                                   UART_TX_TASK_STACK_DEPTH, NULL,
                                   APP_TASK_PRIORITY, g_uart_tx_task_stack,
                                   &g_uart_tx_task_buffer) != NULL);
#if !VOFA_SPEED_PID_TELEMETRY_ENABLE && !GRAY_VOFA_TELEMETRY_ENABLE && !IMU_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(uart_echo_task, "uart", UART_TASK_STACK_DEPTH,
                                   uart_queue, APP_TASK_PRIORITY,
                                   g_uart_task_stack, &g_uart_task_buffer) != NULL);
#endif
#if BUZZER_FEATURE_ENABLE
    configASSERT(xTaskCreateStatic(buzzer_task, "buzzer",
                                   BUZZER_TASK_STACK_DEPTH, NULL,
                                   APP_TASK_PRIORITY, g_buzzer_task_stack,
                                   &g_buzzer_task_buffer) != NULL);
#endif
#if SERVO_FEATURE_ENABLE
    configASSERT(xTaskCreateStatic(servo_task, "servo",
                                   SERVO_TASK_STACK_DEPTH, NULL,
                                   APP_TASK_PRIORITY, g_servo_task_stack,
                                   &g_servo_task_buffer) != NULL);
#endif
}
