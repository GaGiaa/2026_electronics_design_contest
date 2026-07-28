#include "app/app_startup.h"

#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#include "algorithms/line_tracking/line_tracking.h"
#include "algorithms/motor_control/motor_control.h"
#include "app/app_profile.h"
#include "app/app_state.h"
#include "app/app_tasks_io.h"
#include "app/app_tasks_motor.h"
#include "app/app_tasks_sensor.h"
#include "app/app_tasks_telemetry.h"
#include "config/app_config.h"
#include "drivers/buttons/board_buttons.h"
#include "drivers/buzzer/board_buzzer.h"
#include "drivers/encoder/board_encoder.h"
#include "drivers/grayscale/board_grayscale.h"
#include "drivers/hcsr04/board_hcsr04.h"
#include "drivers/servo/board_servo.h"
#include "ti_msp_dl_config.h"

volatile app_hardfault_snapshot_t g_hardfault_snapshot;

static StaticTask_t g_idle_task_buffer;
static StackType_t g_idle_task_stack[configIDLE_TASK_STACK_DEPTH];

void app_hardfault_capture(const uint32_t *stacked_frame,
                           uint32_t exception_return)
    __attribute__((noinline, noreturn));

void app_hardfault_capture(const uint32_t *stacked_frame,
                            uint32_t exception_return)
{
    if (stacked_frame != NULL) {
        g_hardfault_snapshot.stacked_r0 = stacked_frame[0];
        g_hardfault_snapshot.stacked_r1 = stacked_frame[1];
        g_hardfault_snapshot.stacked_r2 = stacked_frame[2];
        g_hardfault_snapshot.stacked_r3 = stacked_frame[3];
        g_hardfault_snapshot.stacked_r12 = stacked_frame[4];
        g_hardfault_snapshot.stacked_lr = stacked_frame[5];
        g_hardfault_snapshot.stacked_pc = stacked_frame[6];
        g_hardfault_snapshot.stacked_xpsr = stacked_frame[7];
        g_hardfault_snapshot.stacked_sp = (uint32_t)stacked_frame;
    }
    g_hardfault_snapshot.exception_return = exception_return;
    g_hardfault_snapshot.cfsr = *((volatile uint32_t *)0xE000ED28UL);
    g_hardfault_snapshot.hfsr = *((volatile uint32_t *)0xE000ED2CUL);
    g_hardfault_snapshot.dfsr = *((volatile uint32_t *)0xE000ED30UL);
    g_hardfault_snapshot.mmfar = *((volatile uint32_t *)0xE000ED34UL);
    g_hardfault_snapshot.bfar = *((volatile uint32_t *)0xE000ED38UL);
    g_hardfault_snapshot.icsr = *((volatile uint32_t *)0xE000ED04UL);
    g_hardfault_snapshot.active = 1U;
    for (;;) {
    }
}

__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile(
        "mov r1, lr\n"
        "movs r2, #4\n"
        "tst r1, r2\n"
        "beq 1f\n"
        "mrs r0, psp\n"
        "b 2f\n"
        "1:\n"
        "mrs r0, msp\n"
        "2:\n"
        "ldr r2, =app_hardfault_capture\n"
        "bx r2\n");
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
    (void)task;
    (void)task_name;
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void app_startup(void)
{
    static const uint16_t grayscale_white[BOARD_GRAYSCALE_CHANNEL_COUNT] =
        {1239U, 2393U, 803U, 709U, 2596U, 2254U, 3040U, 829U};
    static const uint16_t grayscale_black[BOARD_GRAYSCALE_CHANNEL_COUNT] =
        {72U, 64U, 67U, 81U, 97U, 76U, 410U, 76U};

    (void)app_profile_get();
    app_state_init();
    board_buttons_init();
    board_encoder_init();
    board_grayscale_init(grayscale_white, grayscale_black);
    line_tracking_init(&g_line_tracking_state, 0);
    motor_control_init();
    NVIC_EnableIRQ(GPIOA_INT_IRQn);
    NVIC_EnableIRQ(GPIOB_INT_IRQn);
    board_buzzer_init(APP_BUZZER_FREQUENCY_HZ, APP_BUZZER_DUTY_PERCENT);
#if APP_SERVO_FEATURE_ENABLE
    board_servo_init();
#endif

    app_tasks_motor_start();
    app_tasks_io_start();
    app_tasks_sensor_start();
    app_tasks_telemetry_start();
}
