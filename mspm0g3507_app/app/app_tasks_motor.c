#include "app/app_tasks_motor.h"

#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#include "algorithms/motor_control/motor_control.h"
#include "app/app_state.h"
#include "config/app_config.h"
#include "config/crsf_config.h"
#include "drivers/encoder/board_encoder.h"
#include "drivers/motor/board_motor.h"
#if CRSF_REMOTE_CONTROL_ENABLE
#include "protocols/crsf/crsf_control.h"
#endif

static StaticTask_t g_motor_task_buffer;
static StackType_t g_motor_task_stack[APP_MOTOR_TASK_STACK_DEPTH];

static void motor_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(10U);
    board_encoder_sample_t samples[BOARD_MOTOR_COUNT];
#if CRSF_REMOTE_CONTROL_ENABLE
    crsf_control_input_t crsf_input;
    float crsf_targets[BOARD_MOTOR_COUNT];
#endif

    (void)argument;
    for (;;) {
        app_state_encoder_cycle_begin();
        samples[BOARD_MOTOR_FRONT_LEFT] =
            board_encoder_sample(BOARD_MOTOR_FRONT_LEFT);
        samples[BOARD_MOTOR_FRONT_RIGHT] =
            board_encoder_sample(BOARD_MOTOR_FRONT_RIGHT);
        samples[BOARD_MOTOR_REAR_LEFT] =
            board_encoder_sample(BOARD_MOTOR_REAR_LEFT);
        samples[BOARD_MOTOR_REAR_RIGHT] =
            board_encoder_sample(BOARD_MOTOR_REAR_RIGHT);
        app_state_encoder_sample_publish(BOARD_MOTOR_FRONT_LEFT,
                                          samples[BOARD_MOTOR_FRONT_LEFT]);
        app_state_encoder_sample_publish(BOARD_MOTOR_FRONT_RIGHT,
                                          samples[BOARD_MOTOR_FRONT_RIGHT]);
        app_state_encoder_sample_publish(BOARD_MOTOR_REAR_LEFT,
                                          samples[BOARD_MOTOR_REAR_LEFT]);
        app_state_encoder_sample_publish(BOARD_MOTOR_REAR_RIGHT,
                                          samples[BOARD_MOTOR_REAR_RIGHT]);
#if CRSF_REMOTE_CONTROL_ENABLE
        app_state_crsf_snapshot_copy(&crsf_input);
        app_state_crsf_set_link_active(
            crsf_control_mix(&crsf_input,
                             (uint32_t)xTaskGetTickCount(),
                             crsf_targets));
        g_motor_speed_targets_mm_s[BOARD_MOTOR_FRONT_LEFT] =
            crsf_targets[BOARD_MOTOR_FRONT_LEFT];
        g_motor_speed_targets_mm_s[BOARD_MOTOR_FRONT_RIGHT] =
            crsf_targets[BOARD_MOTOR_FRONT_RIGHT];
        g_motor_speed_targets_mm_s[BOARD_MOTOR_REAR_LEFT] =
            crsf_targets[BOARD_MOTOR_REAR_LEFT];
        g_motor_speed_targets_mm_s[BOARD_MOTOR_REAR_RIGHT] =
            crsf_targets[BOARD_MOTOR_REAR_RIGHT];
#endif
        motor_control_step(samples);
        app_state_encoder_cycle_end();

        board_motor_set_signed_duty(
            BOARD_MOTOR_FRONT_LEFT,
            motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT));
        board_motor_set_signed_duty(
            BOARD_MOTOR_FRONT_RIGHT,
            motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_RIGHT));
        board_motor_set_signed_duty(
            BOARD_MOTOR_REAR_LEFT,
            motor_control_get_output_duty_percent(BOARD_MOTOR_REAR_LEFT));
        board_motor_set_signed_duty(
            BOARD_MOTOR_REAR_RIGHT,
            motor_control_get_output_duty_percent(BOARD_MOTOR_REAR_RIGHT));
        vTaskDelayUntil(&last_wake_time, interval);
    }
}

void app_tasks_motor_start(void)
{
    configASSERT(xTaskCreateStatic(motor_task, "motor",
                                   APP_MOTOR_TASK_STACK_DEPTH, NULL,
                                   APP_TASK_PRIORITY, g_motor_task_stack,
                                   &g_motor_task_buffer) != NULL);
}
