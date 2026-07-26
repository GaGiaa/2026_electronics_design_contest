#include "app/app_tasks_io.h"

#include <stdint.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "config/app_config.h"
#include "drivers/buttons/board_buttons.h"
#include "drivers/buzzer/board_buzzer.h"
#include "drivers/oled/board_oled.h"
#include "drivers/servo/board_servo.h"
#include "drivers/uart/board_uart.h"
#include "drivers/ws2812/board_ws2812.h"
#include "app/app_state.h"

#if APP_WS2812_ANIMATION_ENABLE || APP_WS2812_STATUS_INDICATOR_ENABLE
static StaticTask_t g_ws2812_task_buffer;
static StackType_t g_ws2812_task_stack[APP_WS2812_TASK_STACK_DEPTH];
#endif
#if APP_OLED_TEST_TASK_ENABLE
static StaticTask_t g_oled_test_task_buffer;
static StackType_t g_oled_test_task_stack[APP_OLED_TEST_TASK_STACK_DEPTH];
#endif
#if !APP_VOFA_SPEED_PID_TELEMETRY_ENABLE && !APP_GRAY_VOFA_TELEMETRY_ENABLE && !APP_LINE_CONTROL_VOFA_TELEMETRY_ENABLE && !APP_IMU_TELEMETRY_ENABLE
static StaticTask_t g_uart_task_buffer;
static StackType_t g_uart_task_stack[APP_UART_TASK_STACK_DEPTH];
#endif
static StaticTask_t g_uart_tx_task_buffer;
static StackType_t g_uart_tx_task_stack[APP_UART_TX_TASK_STACK_DEPTH];
static StaticTask_t g_button_task_buffer;
static StackType_t g_button_task_stack[APP_BUTTON_TASK_STACK_DEPTH];
static StaticQueue_t g_uart_queue_buffer;
static uint8_t g_uart_queue_storage[APP_UART_RX_QUEUE_LENGTH * sizeof(uint8_t)];

#if APP_WS2812_ANIMATION_ENABLE || APP_WS2812_STATUS_INDICATOR_ENABLE
static void ws2812_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(500U);
    board_ws2812_pixel_t pixels[BOARD_WS2812_PIXEL_COUNT] = {0};
#if APP_WS2812_STATUS_INDICATOR_ENABLE
    app_drive_control_snapshot_t drive_snapshot = {0};
    bool red_blink_on = true;
    uint32_t color_index = 0U;
#else
    uint32_t pixel_index = 0U;
    uint32_t color_index = 0U;
#endif

    (void)argument;
    for (;;) {
        uint32_t index;

        for (index = 0U; index < BOARD_WS2812_PIXEL_COUNT; ++index) {
            pixels[index].red = 0U;
            pixels[index].green = 0U;
            pixels[index].blue = 0U;
        }
#if APP_WS2812_STATUS_INDICATOR_ENABLE
        app_state_drive_control_snapshot_copy(&drive_snapshot);
        if (drive_snapshot.link_active) {
            pixels[0].green = APP_WS2812_BRIGHTNESS;
        } else if (red_blink_on) {
            pixels[0].red = APP_WS2812_BRIGHTNESS;
        }

        if (color_index == 0U) {
            pixels[3].red = APP_WS2812_BRIGHTNESS;
        } else if (color_index == 1U) {
            pixels[3].blue = APP_WS2812_BRIGHTNESS;
        } else if (color_index == 2U) {
            pixels[3].green = APP_WS2812_BRIGHTNESS;
        } else {
            pixels[3].red = APP_WS2812_BRIGHTNESS;
            pixels[3].green = APP_WS2812_BRIGHTNESS;
            pixels[3].blue = APP_WS2812_BRIGHTNESS;
        }
        red_blink_on = drive_snapshot.link_active ? true : !red_blink_on;
#else
        if (color_index == 0U) {
            pixels[pixel_index].red = APP_WS2812_BRIGHTNESS;
        } else if (color_index == 1U) {
            pixels[pixel_index].green = APP_WS2812_BRIGHTNESS;
        } else if (color_index == 2U) {
            pixels[pixel_index].blue = APP_WS2812_BRIGHTNESS;
        } else {
            pixels[pixel_index].red = APP_WS2812_BRIGHTNESS;
            pixels[pixel_index].green = APP_WS2812_BRIGHTNESS;
            pixels[pixel_index].blue = APP_WS2812_BRIGHTNESS;
        }
#endif
        board_ws2812_write(pixels);

#if APP_WS2812_STATUS_INDICATOR_ENABLE
        ++color_index;
        if (color_index == 4U) {
            color_index = 0U;
        }
#else
        ++color_index;
        if (color_index == 4U) {
            color_index = 0U;
            ++pixel_index;
            if (pixel_index == BOARD_WS2812_PIXEL_COUNT) {
                pixel_index = 0U;
            }
        }
#endif
        vTaskDelayUntil(&last_wake_time, interval);
    }
}
#endif

#if APP_OLED_TEST_TASK_ENABLE
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

static void button_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(APP_BUTTON_TASK_INTERVAL_MS);

    (void)argument;
    for (;;) {
        board_buttons_events_t events = board_buttons_scan();
        app_state_buttons_publish(events.stable_pressed_mask,
                                  events.pressed_mask, events.released_mask);
        vTaskDelayUntil(&last_wake_time, interval);
    }
}

#if !APP_VOFA_SPEED_PID_TELEMETRY_ENABLE && !APP_GRAY_VOFA_TELEMETRY_ENABLE && !APP_LINE_CONTROL_VOFA_TELEMETRY_ENABLE && !APP_IMU_TELEMETRY_ENABLE && !APP_BUTTON_VOFA_TELEMETRY_ENABLE
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

#if APP_BUZZER_FEATURE_ENABLE
static StaticTask_t g_buzzer_task_buffer;
static StackType_t g_buzzer_task_stack[APP_BUZZER_TASK_STACK_DEPTH];

static void buzzer_task(void *argument)
{
    (void)argument;
    for (;;) {
        board_buzzer_start();
        vTaskDelay(pdMS_TO_TICKS(APP_BUZZER_ON_TIME_MS));
        board_buzzer_stop();
        vTaskDelay(pdMS_TO_TICKS(APP_BUZZER_OFF_TIME_MS));
    }
}
#endif

#if APP_SERVO_FEATURE_ENABLE
static StaticTask_t g_servo_task_buffer;
static StackType_t g_servo_task_stack[APP_SERVO_TASK_STACK_DEPTH];

static void servo_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();

    (void)argument;
    for (;;) {
        g_servo_pulse_us = board_servo_set_angle_deg(g_servo_angle_deg);
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(APP_SERVO_TASK_INTERVAL_MS));
    }
}
#endif

void app_tasks_io_start(void)
{
    QueueHandle_t uart_queue;

    uart_queue = xQueueCreateStatic(APP_UART_RX_QUEUE_LENGTH, sizeof(uint8_t),
                                     g_uart_queue_storage, &g_uart_queue_buffer);
    configASSERT(uart_queue != NULL);
    board_uart_enable_rx_interrupt(uart_queue);

#if APP_WS2812_ANIMATION_ENABLE || APP_WS2812_STATUS_INDICATOR_ENABLE
    configASSERT(xTaskCreateStatic(ws2812_task, "ws2812",
                                   APP_WS2812_TASK_STACK_DEPTH, NULL,
                                   APP_TASK_PRIORITY, g_ws2812_task_stack,
                                   &g_ws2812_task_buffer) != NULL);
#endif
#if APP_OLED_TEST_TASK_ENABLE
    configASSERT(xTaskCreateStatic(oled_test_task, "oled",
                                   APP_OLED_TEST_TASK_STACK_DEPTH, NULL,
                                   APP_TASK_PRIORITY, g_oled_test_task_stack,
                                   &g_oled_test_task_buffer) != NULL);
#endif
    configASSERT(xTaskCreateStatic(button_task, "buttons",
                                   APP_BUTTON_TASK_STACK_DEPTH, NULL,
                                   APP_TASK_PRIORITY, g_button_task_stack,
                                   &g_button_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(board_uart_tx_task, "uart_tx",
                                   APP_UART_TX_TASK_STACK_DEPTH, NULL,
                                   APP_TASK_PRIORITY, g_uart_tx_task_stack,
                                   &g_uart_tx_task_buffer) != NULL);
#if !APP_VOFA_SPEED_PID_TELEMETRY_ENABLE && !APP_GRAY_VOFA_TELEMETRY_ENABLE && !APP_LINE_CONTROL_VOFA_TELEMETRY_ENABLE && !APP_IMU_TELEMETRY_ENABLE && !APP_BUTTON_VOFA_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(uart_echo_task, "uart", APP_UART_TASK_STACK_DEPTH,
                                   uart_queue, APP_TASK_PRIORITY,
                                   g_uart_task_stack, &g_uart_task_buffer) != NULL);
#endif
#if APP_BUZZER_FEATURE_ENABLE
    configASSERT(xTaskCreateStatic(buzzer_task, "buzzer",
                                   APP_BUZZER_TASK_STACK_DEPTH, NULL,
                                   APP_TASK_PRIORITY, g_buzzer_task_stack,
                                   &g_buzzer_task_buffer) != NULL);
#endif
#if APP_SERVO_FEATURE_ENABLE
    configASSERT(xTaskCreateStatic(servo_task, "servo",
                                   APP_SERVO_TASK_STACK_DEPTH, NULL,
                                   APP_TASK_PRIORITY, g_servo_task_stack,
                                   &g_servo_task_buffer) != NULL);
#endif
}
