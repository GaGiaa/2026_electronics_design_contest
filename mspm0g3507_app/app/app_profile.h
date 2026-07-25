#ifndef APP_PROFILE_H
#define APP_PROFILE_H

#include <stdbool.h>

#include "config/app_config.h"
#include "config/crsf_config.h"

#define APP_TASK_PRIORITY 1U
#define MOTOR_TASK_STACK_DEPTH 256U
#define WS2812_TASK_STACK_DEPTH 256U
#define OLED_TEST_TASK_STACK_DEPTH 384U
#define UART_TASK_STACK_DEPTH 256U
#define UART_TX_TASK_STACK_DEPTH 256U
#define BUZZER_TASK_STACK_DEPTH 128U
#define SERVO_TASK_STACK_DEPTH 128U
#define BUTTON_TASK_STACK_DEPTH 128U
#define BUTTON_FEATURE_ENABLE 0U
#define UART_RX_QUEUE_LENGTH 64U
#define CRSF_TASK_STACK_DEPTH 256U
#define WS2812_BRIGHTNESS 16U
#define BUTTON_TASK_INTERVAL_MS 10U
#define TELEMETRY_TASK_PRIORITY 0U

#if RTOS_MONITOR_ENABLE
#define RTOS_MONITOR_TASK_STACK_DEPTH 512U
#endif

#ifndef VOFA_SPEED_PID_TELEMETRY_ENABLE
#define VOFA_SPEED_PID_TELEMETRY_ENABLE 0U
#endif
#define VOFA_SPEED_PID_TELEMETRY_INTERVAL_MS 10U
#define VOFA_SPEED_PID_TELEMETRY_TASK_STACK_DEPTH 256U
#define VOFA_SPEED_PID_TELEMETRY_TASK_PRIORITY 0U

#ifndef GRAY_VOFA_TELEMETRY_ENABLE
#define GRAY_VOFA_TELEMETRY_ENABLE 0U
#endif
#define GRAY_VOFA_TELEMETRY_INTERVAL_MS 100U
#define GRAY_VOFA_CHANNEL_COUNT 22U

#ifndef IMU_TELEMETRY_ENABLE
#define IMU_TELEMETRY_ENABLE 0U
#endif
#ifndef IMU_YAW_ENABLE
#define IMU_YAW_ENABLE 0U
#endif
#define IMU_YAW_TRACK_WIDTH_MM 130.0f
#define IMU_TASK_STACK_DEPTH 512U
#define IMU_TASK_PRIORITY 0U
#define IMU_SAMPLE_INTERVAL_MS 10U
#define IMU_REINIT_FAILURE_THRESHOLD 3U
#define GRAY_SAMPLE_INTERVAL_MS 10U
#define GRAY_TASK_STACK_DEPTH 512U

#if VOFA_SPEED_PID_TELEMETRY_ENABLE && (IMU_TELEMETRY_ENABLE || GRAY_VOFA_TELEMETRY_ENABLE)
#error "VOFA telemetry modes cannot be enabled together"
#endif
#if IMU_TELEMETRY_ENABLE && GRAY_VOFA_TELEMETRY_ENABLE
#error "IMU and grayscale telemetry cannot be enabled together"
#endif
#if IMU_TELEMETRY_ENABLE && !IMU_YAW_ENABLE
#error "IMU telemetry requires IMU yaw to be enabled"
#endif

#define BUZZER_FEATURE_ENABLE 0U
#define BUZZER_FREQUENCY_HZ 2000U
#define BUZZER_DUTY_PERCENT 50U
#define BUZZER_ON_TIME_MS 200U
#define BUZZER_OFF_TIME_MS 1800U

#if (BUZZER_FREQUENCY_HZ < 1000U) || (BUZZER_FREQUENCY_HZ > 20000U)
#error "BUZZER_FREQUENCY_HZ must be between 1000 Hz and 20000 Hz"
#endif
#if (BUZZER_ON_TIME_MS == 0U) || (BUZZER_OFF_TIME_MS == 0U)
#error "BUZZER_ON_TIME_MS and BUZZER_OFF_TIME_MS must be nonzero"
#endif

typedef struct {
    bool enable_motor_control;
    bool enable_imu;
    bool enable_grayscale;
    bool enable_crsf;
    bool enable_oled;
    bool enable_telemetry;
} app_profile_t;

const app_profile_t *app_profile_get(void);

#endif
