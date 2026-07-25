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
#include "drivers/buzzer/board_buzzer.h"
#include "drivers/encoder/board_encoder.h"
#include "drivers/grayscale/board_grayscale.h"
#include "drivers/servo/board_servo.h"
#include "ti_msp_dl_config.h"

static StaticTask_t g_idle_task_buffer;
static StackType_t g_idle_task_stack[configIDLE_TASK_STACK_DEPTH];

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
        {1843U, 2809U, 852U, 663U, 2720U, 2483U, 3054U, 993U};
    static const uint16_t grayscale_black[BOARD_GRAYSCALE_CHANNEL_COUNT] =
        {965U, 1489U, 121U, 98U, 1264U, 923U, 2709U, 484U};

    (void)app_profile_get();
    app_state_init();
#if APP_BUTTON_FEATURE_ENABLE
    board_buttons_init();
#endif
    board_encoder_init();
    board_grayscale_init(grayscale_white, grayscale_black);
    line_tracking_init(&g_line_tracking_state, 0);
    motor_control_init();
    NVIC_EnableIRQ(GPIOA_INT_IRQn);
    board_buzzer_init(APP_BUZZER_FREQUENCY_HZ, APP_BUZZER_DUTY_PERCENT);
#if APP_SERVO_FEATURE_ENABLE
    board_servo_init();
#endif

    app_tasks_motor_start();
    app_tasks_io_start();
    app_tasks_sensor_start();
    app_tasks_telemetry_start();
}
