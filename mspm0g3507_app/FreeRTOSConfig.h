#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configCPU_CLOCK_HZ                         (32000000UL)
#define configTICK_RATE_HZ                          1000U
#define configENABLE_MPU                            0
#define configUSE_PREEMPTION                        1
#define configUSE_TIME_SLICING                      1
#define configMAX_PRIORITIES                        4U
#define configMINIMAL_STACK_SIZE                    128U
#define configMAX_TASK_NAME_LEN                     16U
#define configTICK_TYPE_WIDTH_IN_BITS               TICK_TYPE_WIDTH_32_BITS
#define configUSE_TICKLESS_IDLE                     0
#define configUSE_TIMERS                            0
#define configUSE_EVENT_GROUPS                      0
#define configUSE_STREAM_BUFFERS                    0
#define configUSE_CO_ROUTINES                       0
#define configSUPPORT_STATIC_ALLOCATION             1
#define configSUPPORT_DYNAMIC_ALLOCATION            0
#define configCHECK_FOR_STACK_OVERFLOW              2
#define configUSE_MALLOC_FAILED_HOOK                1
#define configUSE_IDLE_HOOK                         0
#define configUSE_TICK_HOOK                         0
#define configIDLE_TASK_STACK_DEPTH                 configMINIMAL_STACK_SIZE
#define configPRIO_BITS                             2U
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY     3U
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 1U
#define configKERNEL_INTERRUPT_PRIORITY (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))
#define INCLUDE_vTaskDelay                          1
#define INCLUDE_vTaskDelayUntil                     1
#define configASSERT(value) do { if ((value) == 0) { taskDISABLE_INTERRUPTS(); for (;;) { } } } while (0)

#endif
