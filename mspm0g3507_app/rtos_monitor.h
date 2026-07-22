#ifndef RTOS_MONITOR_H
#define RTOS_MONITOR_H

#include <stdint.h>

#ifndef configMAX_TASK_NAME_LEN
#define configMAX_TASK_NAME_LEN 16U
#endif

#define RTOS_MONITOR_MAX_TASKS 16U
#define RTOS_MONITOR_TIMER_HZ 10000000UL
#define RTOS_MONITOR_SAMPLE_INTERVAL_MS 1000U

typedef struct {
    char name[configMAX_TASK_NAME_LEN]; /* 任务名称。 */
    uint32_t state;                     /* 任务当前状态，对应 eTaskState：0=eRunning（运行），1=eReady（就绪），2=eBlocked（阻塞），3=eSuspended（挂起），4=eDeleted（已删除待释放）。 */
    uint32_t priority;                  /* 任务当前优先级。 */
    uint32_t runtime_us;                /* 统计窗口内的任务运行时间，单位为微秒。 */
    uint32_t runtime_percent_x100;      /* 任务运行时间占比，实际百分比乘以 100。 */
    uint32_t stack_high_water_words;    /* 栈历史最小剩余量，单位为 StackType_t 字数。 */
} rtos_monitor_task_snapshot_t;

typedef struct {
    uint32_t sequence;       /* 快照更新序号：奇数表示正在更新，偶数表示更新完成。 */
    uint32_t window_us;      /* 本次 CPU 统计窗口长度，单位为微秒。 */
    uint32_t timer_hz;       /* 运行时间统计计时器的实际频率，单位为 Hz。 */
    uint32_t cpu_percent_x100;  /* CPU 总利用率，实际百分比乘以 100。 */
    uint32_t idle_percent_x100; /* 空闲任务占用率，实际百分比乘以 100。 */
    uint32_t task_count;      /* 当前快照中的任务数量。 */
    rtos_monitor_task_snapshot_t tasks[RTOS_MONITOR_MAX_TASKS]; /* 各任务统计信息。 */
} rtos_monitor_snapshot_t;

extern volatile rtos_monitor_snapshot_t g_rtos_monitor_snapshot;

uint32_t rtos_monitor_counter_delta(uint32_t newer, uint32_t older);
uint32_t rtos_monitor_percent_x100(uint32_t part, uint32_t total);

void rtos_monitor_runtime_timer_init(void);
uint32_t rtos_monitor_runtime_timer_now(void);
void rtos_monitor_task(void *argument);

#endif
