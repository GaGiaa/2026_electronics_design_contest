#include "drivers/hcsr04/board_hcsr04.h"

#include <stddef.h>

#include <FreeRTOS.h>

#include "config/rtos_monitor_config.h"
#include "services/rtos_monitor/rtos_monitor.h"
#include "ti_msp_dl_config.h"

#define BOARD_HCSR04_TRIG_PULSE_CYCLES 800U

static TaskHandle_t g_task_handle;
static volatile bool g_echo_active;
static volatile bool g_result_ready;
static volatile uint32_t g_rising_edge_ticks;
static volatile uint32_t g_echo_ticks;

void board_hcsr04_init(void)
{
    rtos_monitor_runtime_timer_init();
    g_task_handle = NULL;
    g_echo_active = false;
    g_result_ready = false;
    g_rising_edge_ticks = 0U;
    g_echo_ticks = 0U;
    DL_GPIO_clearPins(HCSR04_PORT, HCSR04_TRIG_PIN);
    DL_GPIO_disableInterrupt(HCSR04_PORT, HCSR04_ECHO_PIN);
    DL_GPIO_clearInterruptStatus(HCSR04_PORT, HCSR04_ECHO_PIN);
}

void board_hcsr04_set_task_handle(TaskHandle_t task_handle)
{
    g_task_handle = task_handle;
    DL_GPIO_clearInterruptStatus(HCSR04_PORT, HCSR04_ECHO_PIN);
    DL_GPIO_enableInterrupt(HCSR04_PORT, HCSR04_ECHO_PIN);
}

void board_hcsr04_trigger(void)
{
    taskENTER_CRITICAL();
    g_echo_active = false;
    g_result_ready = false;
    g_echo_ticks = 0U;
    taskEXIT_CRITICAL();

    DL_GPIO_disableInterrupt(HCSR04_PORT, HCSR04_ECHO_PIN);
    DL_GPIO_clearInterruptStatus(HCSR04_PORT, HCSR04_ECHO_PIN);
    DL_GPIO_setPins(HCSR04_PORT, HCSR04_TRIG_PIN);
    DL_Common_delayCycles(BOARD_HCSR04_TRIG_PULSE_CYCLES);
    DL_GPIO_clearPins(HCSR04_PORT, HCSR04_TRIG_PIN);
    DL_GPIO_enableInterrupt(HCSR04_PORT, HCSR04_ECHO_PIN);
}

void board_hcsr04_abort(void)
{
    taskENTER_CRITICAL();
    g_echo_active = false;
    g_result_ready = false;
    g_echo_ticks = 0U;
    taskEXIT_CRITICAL();

    DL_GPIO_disableInterrupt(HCSR04_PORT, HCSR04_ECHO_PIN);
    DL_GPIO_clearInterruptStatus(HCSR04_PORT, HCSR04_ECHO_PIN);
}

bool board_hcsr04_read_result(board_hcsr04_result_t *result)
{
    if (result == NULL) {
        return false;
    }

    taskENTER_CRITICAL();
    result->valid = g_result_ready;
    result->echo_ticks = g_echo_ticks;
    g_result_ready = false;
    taskEXIT_CRITICAL();
    return true;
}

void board_hcsr04_gpio_irq_handler(void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (DL_GPIO_getPendingInterrupt(HCSR04_PORT) != HCSR04_ECHO_IIDX) {
        return;
    }

    DL_GPIO_clearInterruptStatus(HCSR04_PORT, HCSR04_ECHO_PIN);

    if (DL_GPIO_readPins(HCSR04_PORT, HCSR04_ECHO_PIN) != 0U) {
        g_rising_edge_ticks = rtos_monitor_runtime_timer_now();
        g_echo_active = true;
    } else if (g_echo_active) {
        g_echo_ticks = rtos_monitor_counter_delta(
            rtos_monitor_runtime_timer_now(), g_rising_edge_ticks);
        g_result_ready = true;
        g_echo_active = false;
        DL_GPIO_disableInterrupt(HCSR04_PORT, HCSR04_ECHO_PIN);
        if (g_task_handle != NULL) {
            vTaskNotifyGiveFromISR(g_task_handle, &higher_priority_task_woken);
            portYIELD_FROM_ISR(higher_priority_task_woken);
        }
    }
}
