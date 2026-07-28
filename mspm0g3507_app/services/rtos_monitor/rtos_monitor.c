#include "rtos_monitor.h"

#include <stddef.h>

uint32_t rtos_monitor_counter_delta(uint32_t newer, uint32_t older)
{
    return newer - older;
}

uint32_t rtos_monitor_percent_x100(uint32_t part, uint32_t total)
{
    uint64_t percent;

    if (total == 0U) {
        return 0U;
    }
    percent = ((uint64_t)part * 10000ULL) / total;
    if (percent > 10000ULL) {
        percent = 10000ULL;
    }
    return (uint32_t)percent;
}

#if !defined(RTOS_MONITOR_HOST_TEST)

#include <FreeRTOS.h>
#include <task.h>

#include "ti_msp_dl_config.h"

volatile rtos_monitor_snapshot_t g_rtos_monitor_snapshot;

void rtos_monitor_runtime_timer_init(void)
{
    DL_Timer_stopCounter(RTOS_MONITOR_TIMER_INST);
    DL_Timer_setTimerCount(RTOS_MONITOR_TIMER_INST, 0U);
    DL_Timer_startCounter(RTOS_MONITOR_TIMER_INST);
}

uint32_t rtos_monitor_runtime_timer_now(void)
{
    return DL_Timer_getTimerCount(RTOS_MONITOR_TIMER_INST);
}

#if APP_RTOS_MONITOR_ENABLE

static TaskStatus_t g_previous_task_status[RTOS_MONITOR_MAX_TASKS];
static TaskStatus_t g_current_task_status[RTOS_MONITOR_MAX_TASKS];
static UBaseType_t g_previous_task_count;
static configRUN_TIME_COUNTER_TYPE g_previous_total_runtime;

static uint32_t runtime_ticks_to_us(configRUN_TIME_COUNTER_TYPE ticks)
{
    return (uint32_t)(((uint64_t)ticks * 1000000ULL) /
                      RTOS_MONITOR_TIMER_HZ);
}

static const TaskStatus_t *find_previous_task(TaskHandle_t handle)
{
    UBaseType_t index;

    for (index = 0U; index < g_previous_task_count; ++index) {
        if (g_previous_task_status[index].xHandle == handle) {
            return &g_previous_task_status[index];
        }
    }
    return NULL;
}

static void publish_snapshot(UBaseType_t task_count,
                             configRUN_TIME_COUNTER_TYPE total_delta,
                             configRUN_TIME_COUNTER_TYPE idle_delta)
{
    uint32_t busy_runtime;
    uint32_t index;

    ++g_rtos_monitor_snapshot.sequence;
    g_rtos_monitor_snapshot.window_us = runtime_ticks_to_us(total_delta);
    g_rtos_monitor_snapshot.timer_hz = RTOS_MONITOR_TIMER_HZ;
    g_rtos_monitor_snapshot.idle_percent_x100 =
        rtos_monitor_percent_x100((uint32_t)idle_delta, (uint32_t)total_delta);
    busy_runtime = ((uint32_t)total_delta > (uint32_t)idle_delta)
                       ? ((uint32_t)total_delta - (uint32_t)idle_delta)
                       : 0U;
    g_rtos_monitor_snapshot.cpu_percent_x100 =
        rtos_monitor_percent_x100(busy_runtime, (uint32_t)total_delta);
    g_rtos_monitor_snapshot.task_count = task_count;

    for (index = 0U; index < RTOS_MONITOR_MAX_TASKS; ++index) {
        volatile rtos_monitor_task_snapshot_t *snapshot =
            &g_rtos_monitor_snapshot.tasks[index];
        if (index >= task_count) {
            snapshot->name[0] = '\0';
            snapshot->state = 0U;
            snapshot->priority = 0U;
            snapshot->runtime_us = 0U;
            snapshot->runtime_percent_x100 = 0U;
            snapshot->stack_high_water_words = 0U;
            continue;
        }

        const TaskStatus_t *previous =
            find_previous_task(g_current_task_status[index].xHandle);
        configRUN_TIME_COUNTER_TYPE runtime_delta =
            (previous == NULL)
                ? g_current_task_status[index].ulRunTimeCounter
                : rtos_monitor_counter_delta(
                      g_current_task_status[index].ulRunTimeCounter,
                      previous->ulRunTimeCounter);
        snapshot->name[0] = '\0';
        for (uint32_t name_index = 0U;
              name_index < RTOS_MONITOR_TASK_NAME_LENGTH;
             ++name_index) {
            snapshot->name[name_index] = '\0';
        }
        for (uint32_t name_index = 0U;
             (name_index + 1U < RTOS_MONITOR_TASK_NAME_LENGTH) &&
             (g_current_task_status[index].pcTaskName != NULL) &&
             (g_current_task_status[index].pcTaskName[name_index] != '\0');
             ++name_index) {
            snapshot->name[name_index] =
                g_current_task_status[index].pcTaskName[name_index];
        }
        snapshot->state = (uint32_t)g_current_task_status[index].eCurrentState;
        snapshot->priority = (uint32_t)g_current_task_status[index].uxCurrentPriority;
        snapshot->runtime_us = runtime_ticks_to_us(runtime_delta);
        snapshot->runtime_percent_x100 =
            rtos_monitor_percent_x100((uint32_t)runtime_delta,
                                       (uint32_t)total_delta);
        snapshot->stack_high_water_words =
            (uint32_t)g_current_task_status[index].usStackHighWaterMark;
    }

    g_rtos_monitor_snapshot.sequence++;
}

void rtos_monitor_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(RTOS_MONITOR_SAMPLE_INTERVAL_MS);
    configRUN_TIME_COUNTER_TYPE current_total_runtime;

    (void)argument;
    g_previous_task_count = uxTaskGetSystemState(
        g_previous_task_status, RTOS_MONITOR_MAX_TASKS, &g_previous_total_runtime);

    for (;;) {
        UBaseType_t current_task_count;
        configRUN_TIME_COUNTER_TYPE total_delta;
        configRUN_TIME_COUNTER_TYPE idle_delta = 0U;
        TaskHandle_t idle_task = xTaskGetIdleTaskHandle();

        vTaskDelayUntil(&last_wake_time, interval);
        current_task_count = uxTaskGetSystemState(
            g_current_task_status, RTOS_MONITOR_MAX_TASKS, &current_total_runtime);
        total_delta = rtos_monitor_counter_delta(current_total_runtime,
                                                 g_previous_total_runtime);
        for (UBaseType_t index = 0U; index < current_task_count; ++index) {
            if (g_current_task_status[index].xHandle == idle_task) {
                const TaskStatus_t *previous =
                    find_previous_task(idle_task);
                idle_delta = (previous == NULL)
                                 ? g_current_task_status[index].ulRunTimeCounter
                                 : rtos_monitor_counter_delta(
                                       g_current_task_status[index].ulRunTimeCounter,
                                       previous->ulRunTimeCounter);
                break;
            }
        }

        if (current_task_count > RTOS_MONITOR_MAX_TASKS) {
            current_task_count = RTOS_MONITOR_MAX_TASKS;
        }
        publish_snapshot(current_task_count, total_delta, idle_delta);
        for (UBaseType_t index = 0U; index < current_task_count; ++index) {
            g_previous_task_status[index] = g_current_task_status[index];
        }
        g_previous_task_count = current_task_count;
        g_previous_total_runtime = current_total_runtime;
    }
}

#else

void rtos_monitor_task(void *argument) { (void)argument; }

#endif

#endif
