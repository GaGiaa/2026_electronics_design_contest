#include <stdint.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "board_encoder.h"
#include "board_bmi160.h"
#include "board_imu_yaw.h"
#include "board_buttons.h"
#include "board_buzzer.h"
#include "board_crsf_uart.h"
#include "board_grayscale.h"
#include "board_motor.h"
#include "board_uart.h"
#include "board_ws2812.h"
#include "crsf_control.h"
#include "crsf_protocol.h"
#include "line_tracking.h"
#include "motor_control.h"
#include "ti_msp_dl_config.h"
#include "vofa_justfloat.h"

/* 任务基础配置 */
/* 普通应用任务的默认优先级。 */
#define APP_TASK_PRIORITY 1U
/* 电机控制任务的栈深度，单位为 StackType_t。 */
#define MOTOR_TASK_STACK_DEPTH 256U
/* WS2812 灯效任务的栈深度。 */
#define WS2812_TASK_STACK_DEPTH 256U
/* UART 接收任务的栈深度。 */
#define UART_TASK_STACK_DEPTH 256U
/* UART 发送任务的栈深度。 */
#define UART_TX_TASK_STACK_DEPTH 256U
/* 蜂鸣器任务的栈深度。 */
#define BUZZER_TASK_STACK_DEPTH 128U
/* 按键扫描任务的栈深度。 */
#define BUTTON_TASK_STACK_DEPTH 128U
/* 按键功能开关，0 表示不创建按键任务。 */
#define BUTTON_FEATURE_ENABLE 0U
/* UART 接收消息队列的最大元素数量。 */
#define UART_RX_QUEUE_LENGTH 64U
#define CRSF_TASK_STACK_DEPTH 256U
/* WS2812 的显示亮度，取值范围由驱动实现约束。 */
#define WS2812_BRIGHTNESS 16U
/* 按键扫描任务的执行周期，单位为毫秒。 */
#define BUTTON_TASK_INTERVAL_MS 10U

/* 遥测任务的默认优先级。 */
#define TELEMETRY_TASK_PRIORITY 0U

/* 速度 PID 遥测 */
#ifndef VOFA_SPEED_PID_TELEMETRY_ENABLE
/* 速度 PID 的 VOFA+ 遥测开关，允许由构建脚本覆盖。 */
#define VOFA_SPEED_PID_TELEMETRY_ENABLE 0U
#endif
/* 速度 PID 遥测发送周期，单位为毫秒。 */
#define VOFA_SPEED_PID_TELEMETRY_INTERVAL_MS 10U
/* 速度 PID 遥测任务的栈深度。 */
#define VOFA_SPEED_PID_TELEMETRY_TASK_STACK_DEPTH 256U
/* 速度 PID 遥测任务的优先级。 */
#define VOFA_SPEED_PID_TELEMETRY_TASK_PRIORITY 0U

/* 灰度遥测 */
#ifndef GRAY_VOFA_TELEMETRY_ENABLE
/* 灰度传感器 VOFA+ 遥测开关，允许由构建脚本覆盖。 */
#define GRAY_VOFA_TELEMETRY_ENABLE 0U
#endif
/* 灰度遥测发送周期，单位为毫秒。 */
#define GRAY_VOFA_TELEMETRY_INTERVAL_MS 100U
/* 灰度遥测帧中的通道数量。 */
#define GRAY_VOFA_CHANNEL_COUNT 22U

/* IMU yaw 与采样任务 */
#ifndef IMU_TELEMETRY_ENABLE
/* IMU yaw 的 VOFA+ 遥测开关，允许由构建脚本覆盖。 */
#define IMU_TELEMETRY_ENABLE 0U
#endif
#ifndef IMU_YAW_ENABLE
/* IMU yaw 解算功能开关，允许由构建脚本覆盖。 */
#define IMU_YAW_ENABLE 0U
#endif
/* 车辆左右轮中心之间的实际轮距，单位为毫米。 */
#define IMU_YAW_TRACK_WIDTH_MM 130.0f
/* IMU 采样任务的栈深度。 */
#define IMU_TASK_STACK_DEPTH 512U
/* IMU 采样任务的优先级。 */
#define IMU_TASK_PRIORITY 0U
/* IMU 采样与 yaw 更新周期，单位为毫秒。 */
#define IMU_SAMPLE_INTERVAL_MS 10U
/* 连续读取失败达到该次数后重新初始化 BMI160。 */
#define IMU_REINIT_FAILURE_THRESHOLD 3U
/* 灰度采样任务的执行周期，单位为毫秒。 */
#define GRAY_SAMPLE_INTERVAL_MS 10U
/* 灰度采样任务的栈深度。 */
#define GRAY_TASK_STACK_DEPTH 512U

/* 遥测互斥检查 */
/* 速度、IMU 和灰度遥测不能同时占用 UART0。 */
#if VOFA_SPEED_PID_TELEMETRY_ENABLE && (IMU_TELEMETRY_ENABLE || GRAY_VOFA_TELEMETRY_ENABLE)
#error "VOFA telemetry modes cannot be enabled together"
#endif
/* IMU 遥测和灰度遥测不能同时占用 UART0。 */
#if IMU_TELEMETRY_ENABLE && GRAY_VOFA_TELEMETRY_ENABLE
#error "IMU and grayscale telemetry cannot be enabled together"
#endif
/* 只有启用 IMU yaw 解算后，才能发送 IMU yaw 遥测。 */
#if IMU_TELEMETRY_ENABLE && !IMU_YAW_ENABLE
#error "IMU telemetry requires IMU yaw to be enabled"
#endif

/* 蜂鸣器 */
/* 蜂鸣器功能开关，0 表示默认关闭。 */
#define BUZZER_FEATURE_ENABLE 0U
/* 蜂鸣器输出频率，单位为赫兹。 */
#define BUZZER_FREQUENCY_HZ 2000U
/* 蜂鸣器 PWM 占空比，单位为百分比。 */
#define BUZZER_DUTY_PERCENT 50U
/* 蜂鸣器开启持续时间，单位为毫秒。 */
#define BUZZER_ON_TIME_MS 200U
/* 蜂鸣器关闭持续时间，单位为毫秒。 */
#define BUZZER_OFF_TIME_MS 1800U

#if (BUZZER_FREQUENCY_HZ < 1000U) || (BUZZER_FREQUENCY_HZ > 20000U)
#error "BUZZER_FREQUENCY_HZ must be between 1000 Hz and 20000 Hz"
#endif

#if (BUZZER_ON_TIME_MS == 0U) || (BUZZER_OFF_TIME_MS == 0U)
#error "BUZZER_ON_TIME_MS and BUZZER_OFF_TIME_MS must be nonzero"
#endif

static StaticTask_t g_motor_task_buffer;
static StackType_t g_motor_task_stack[MOTOR_TASK_STACK_DEPTH];
static StaticTask_t g_ws2812_task_buffer;
static StackType_t g_ws2812_task_stack[WS2812_TASK_STACK_DEPTH];
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
#if VOFA_SPEED_PID_TELEMETRY_ENABLE
static StaticTask_t g_telemetry_task_buffer;
static StackType_t g_telemetry_task_stack[VOFA_SPEED_PID_TELEMETRY_TASK_STACK_DEPTH];
#endif
static StaticTask_t g_imu_task_buffer;
static StackType_t g_imu_task_stack[IMU_TASK_STACK_DEPTH];
static StaticTask_t g_gray_task_buffer;
static StackType_t g_gray_task_stack[GRAY_TASK_STACK_DEPTH];
#if GRAY_VOFA_TELEMETRY_ENABLE
static StaticTask_t g_gray_telemetry_task_buffer;
static StackType_t g_gray_telemetry_task_stack[GRAY_TASK_STACK_DEPTH];
#endif
#if BUZZER_FEATURE_ENABLE
static StaticTask_t g_buzzer_task_buffer;
static StackType_t g_buzzer_task_stack[BUZZER_TASK_STACK_DEPTH];
#endif
#if CRSF_REMOTE_CONTROL_ENABLE
static StaticTask_t g_crsf_task_buffer;
static StackType_t g_crsf_task_stack[CRSF_TASK_STACK_DEPTH];
#endif
static StaticQueue_t g_uart_queue_buffer;
static uint8_t g_uart_queue_storage[UART_RX_QUEUE_LENGTH * sizeof(uint8_t)];
static StaticTask_t g_idle_task_buffer;
static StackType_t g_idle_task_stack[configIDLE_TASK_STACK_DEPTH];
volatile board_encoder_sample_t g_encoder_samples[BOARD_MOTOR_COUNT];
volatile board_grayscale_snapshot_t g_grayscale_snapshot;
static line_tracking_state_t g_line_tracking_state;
static volatile uint32_t g_grayscale_publish_sequence;
static volatile uint32_t g_encoder_sample_sequence;
#if CRSF_REMOTE_CONTROL_ENABLE
typedef struct {
    crsf_channels_t channels;
    bool link_active;
    uint32_t last_valid_time_ms;
    uint32_t valid_frame_count;
    uint32_t crc_error_count;
    uint32_t frame_error_count;
    uint32_t rx_overflow_count;
} crsf_debug_state_t;

volatile crsf_debug_state_t g_crsf_debug;
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
#endif

void UART_0_INST_IRQHandler(void) { board_uart_irq_handler(); }
void UART_3_INST_IRQHandler(void) { board_crsf_uart_irq_handler(); }
void GROUP1_IRQHandler(void)
{
    switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
    case ENCODER_GPIOA_INT_IIDX:
        board_encoder_gpioa_irq_handler();
        break;
    case ENCODER_GPIOB_INT_IIDX:
        board_encoder_gpiob_irq_handler();
        break;
    default:
        break;
    }
}

#if CRSF_REMOTE_CONTROL_ENABLE
static void crsf_snapshot_copy(crsf_control_input_t *input)
{
    uint32_t channel;

    taskENTER_CRITICAL();
    input->valid = g_crsf_debug.valid_frame_count != 0U;
    input->last_valid_time_ms = g_crsf_debug.last_valid_time_ms;
    for (channel = 0U; channel < CRSF_CHANNEL_COUNT; ++channel) {
        input->channels[channel] = g_crsf_debug.channels.channels[channel];
    }
    taskEXIT_CRITICAL();
}

static void crsf_task(void *argument)
{
    crsf_parser_t parser;
    crsf_channels_t channels;
    uint8_t byte;

    (void)argument;
    board_crsf_uart_enable_rx_interrupt();
    crsf_parser_init(&parser);
    for (;;) {
        if (board_crsf_uart_read_byte(&byte)) {
            do {
                if (crsf_parser_feed(&parser, byte, &channels)) {
                    uint32_t now = (uint32_t)xTaskGetTickCount();

                    taskENTER_CRITICAL();
                    g_crsf_debug.channels = channels;
                    g_crsf_debug.last_valid_time_ms = now;
                    ++g_crsf_debug.valid_frame_count;
                    taskEXIT_CRITICAL();
                }
                g_crsf_debug.crc_error_count = parser.crc_error_count;
                g_crsf_debug.frame_error_count = parser.frame_error_count;
                g_crsf_debug.rx_overflow_count = board_crsf_uart_rx_overflow_count();
            } while (board_crsf_uart_read_byte(&byte));
        } else {
            vTaskDelay(pdMS_TO_TICKS(1U));
        }
    }
}
#endif

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
        ++g_encoder_sample_sequence;
        samples[BOARD_MOTOR_FRONT_LEFT] = board_encoder_sample(BOARD_MOTOR_FRONT_LEFT);
        samples[BOARD_MOTOR_FRONT_RIGHT] = board_encoder_sample(BOARD_MOTOR_FRONT_RIGHT);
        samples[BOARD_MOTOR_REAR_LEFT] = board_encoder_sample(BOARD_MOTOR_REAR_LEFT);
        samples[BOARD_MOTOR_REAR_RIGHT] = board_encoder_sample(BOARD_MOTOR_REAR_RIGHT);
        g_encoder_samples[BOARD_MOTOR_FRONT_LEFT] = samples[BOARD_MOTOR_FRONT_LEFT];
        g_encoder_samples[BOARD_MOTOR_FRONT_RIGHT] = samples[BOARD_MOTOR_FRONT_RIGHT];
        g_encoder_samples[BOARD_MOTOR_REAR_LEFT] = samples[BOARD_MOTOR_REAR_LEFT];
        g_encoder_samples[BOARD_MOTOR_REAR_RIGHT] = samples[BOARD_MOTOR_REAR_RIGHT];
#if CRSF_REMOTE_CONTROL_ENABLE
        crsf_snapshot_copy(&crsf_input);
        g_crsf_debug.link_active = crsf_control_mix(&crsf_input,
                                                    (uint32_t)xTaskGetTickCount(),
                                                    crsf_targets);
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
        ++g_encoder_sample_sequence;

        board_motor_set_signed_duty(BOARD_MOTOR_FRONT_LEFT,
                                    motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT));
        board_motor_set_signed_duty(BOARD_MOTOR_FRONT_RIGHT,
                                    motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_RIGHT));
        board_motor_set_signed_duty(BOARD_MOTOR_REAR_LEFT,
                                    motor_control_get_output_duty_percent(BOARD_MOTOR_REAR_LEFT));
        board_motor_set_signed_duty(BOARD_MOTOR_REAR_RIGHT,
                                    motor_control_get_output_duty_percent(BOARD_MOTOR_REAR_RIGHT));
        vTaskDelayUntil(&last_wake_time, interval);
    }
}

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

#if BUTTON_FEATURE_ENABLE
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

static void motor_control_snapshot_copy(motor_control_wheel_status_t control[BOARD_MOTOR_COUNT])
{
    uint32_t begin_sequence;
    uint32_t end_sequence;
    uint32_t wheel;

    for (;;) {
        begin_sequence = g_encoder_sample_sequence;
        if ((begin_sequence & 1U) == 0U) {
            for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
                control[wheel] = g_motor_control_status[wheel];
            }
            end_sequence = g_encoder_sample_sequence;
            if ((begin_sequence == end_sequence) && ((end_sequence & 1U) == 0U)) {
                break;
            }
        }
    }
}

static void encoder_samples_snapshot_copy(board_encoder_sample_t samples[BOARD_MOTOR_COUNT])
{
    uint32_t begin_sequence;
    uint32_t end_sequence;
    uint32_t wheel;

    for (;;) {
        begin_sequence = g_encoder_sample_sequence;
        if ((begin_sequence & 1U) == 0U) {
            for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
                samples[wheel] = g_encoder_samples[wheel];
            }
            end_sequence = g_encoder_sample_sequence;
            if ((begin_sequence == end_sequence) && ((end_sequence & 1U) == 0U)) {
                break;
            }
        }
    }
}

#if VOFA_SPEED_PID_TELEMETRY_ENABLE
static void telemetry_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(VOFA_SPEED_PID_TELEMETRY_INTERVAL_MS);
    motor_control_wheel_status_t control[BOARD_MOTOR_COUNT];
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(VOFA_JUSTFLOAT_CHANNEL_COUNT)];

    (void)argument;
    for (;;) {
        board_motor_wheel_t wheel = g_motor_debug.wheel;

        motor_control_snapshot_copy(control);
        if (wheel >= BOARD_MOTOR_COUNT) {
            wheel = BOARD_MOTOR_FRONT_LEFT;
        }
        if (vofa_justfloat_encode4(frame, sizeof(frame),
                                   control[wheel].target_speed_mm_per_s,
                                   control[wheel].instant_feedback_speed_mm_per_s,
                                   control[wheel].feedback_speed_mm_per_s,
                                   control[wheel].output_duty_percent)) {
            board_uart_write(frame, sizeof(frame));
        }
        vTaskDelayUntil(&last_wake_time, interval);
    }
}
#endif

static void imu_task(void *argument)
{
    TickType_t last_wake_time;
    const TickType_t interval = pdMS_TO_TICKS(IMU_SAMPLE_INTERVAL_MS);
    board_bmi160_sample_t sample;
    board_bmi160_status_t status;
    uint8_t chip_id;
    uint32_t consecutive_failures;
#if IMU_YAW_ENABLE
    board_imu_yaw_state_t yaw_state;
    board_encoder_sample_t encoder_samples[BOARD_MOTOR_COUNT];
#endif
#if IMU_TELEMETRY_ENABLE && IMU_YAW_ENABLE
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(3U)];
#endif

    (void)argument;
#if IMU_YAW_ENABLE
    board_imu_yaw_init(&yaw_state, IMU_YAW_TRACK_WIDTH_MM);
#endif
    for (;;) {
        chip_id = 0U;
        status = board_bmi160_init(&chip_id);
/* BMI160 initialization is intentionally silent; telemetry uses JustFloat below. */
        if (status != BOARD_BMI160_STATUS_OK) {
            vTaskDelay(pdMS_TO_TICKS(1000U));
        } else {
            consecutive_failures = 0U;
            last_wake_time = xTaskGetTickCount();
            for (;;) {
                status = board_bmi160_read_sample(&sample);
                if (status != BOARD_BMI160_STATUS_OK) {
                    ++consecutive_failures;
                    if (consecutive_failures >= IMU_REINIT_FAILURE_THRESHOLD) {
                        break;
                    }
                } else {
                    consecutive_failures = 0U;
#if IMU_YAW_ENABLE
                    encoder_samples_snapshot_copy(encoder_samples);
                    board_imu_yaw_update(&yaw_state, &sample, encoder_samples,
                                         (float)IMU_SAMPLE_INTERVAL_MS / 1000.0f);
#endif
#if IMU_TELEMETRY_ENABLE && IMU_YAW_ENABLE
                    if (vofa_justfloat_encode3(frame, sizeof(frame), yaw_state.yaw_deg, yaw_state.yaw_rate_dps, yaw_state.gyro_bias_z_dps)) {
                        board_uart_write(frame, sizeof(frame));
                    }
#endif
                }
                vTaskDelayUntil(&last_wake_time, interval);
            }
        }
    }
}

static void grayscale_snapshot_copy(board_grayscale_snapshot_t *snapshot)
{
    uint32_t begin_sequence;
    uint32_t end_sequence;
    uint32_t channel;

    for (;;) {
        begin_sequence = g_grayscale_publish_sequence;
        if ((begin_sequence & 1U) == 0U) {
            for (channel = 0U; channel < BOARD_GRAYSCALE_CHANNEL_COUNT; ++channel) {
                snapshot->raw[channel] = g_grayscale_snapshot.raw[channel];
                snapshot->normalized[channel] = g_grayscale_snapshot.normalized[channel];
            }
            snapshot->digital = g_grayscale_snapshot.digital;
            snapshot->black_mask = g_grayscale_snapshot.black_mask;
            snapshot->black_count = g_grayscale_snapshot.black_count;
            snapshot->line_strength = g_grayscale_snapshot.line_strength;
            snapshot->line_error = g_grayscale_snapshot.line_error;
            snapshot->sequence = g_grayscale_snapshot.sequence;
            end_sequence = g_grayscale_publish_sequence;
            if ((begin_sequence == end_sequence) && ((end_sequence & 1U) == 0U)) {
                break;
            }
        }
    }
}

static void gray_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(GRAY_SAMPLE_INTERVAL_MS);
    board_grayscale_snapshot_t sample;
    line_tracking_result_t tracking;
    uint32_t channel;

    (void)argument;
    for (;;) {
        board_grayscale_sample(&sample);
        line_tracking_update(&g_line_tracking_state, sample.normalized,
                             sample.digital, &tracking);
        sample.black_mask = tracking.black_mask;
        sample.black_count = tracking.black_count;
        sample.line_strength = tracking.line_strength;
        sample.line_error = tracking.line_error;
        ++g_grayscale_publish_sequence;
        for (channel = 0U; channel < BOARD_GRAYSCALE_CHANNEL_COUNT; ++channel) {
            g_grayscale_snapshot.raw[channel] = sample.raw[channel];
            g_grayscale_snapshot.normalized[channel] = sample.normalized[channel];
        }
        g_grayscale_snapshot.digital = sample.digital;
        g_grayscale_snapshot.black_mask = sample.black_mask;
        g_grayscale_snapshot.black_count = sample.black_count;
        g_grayscale_snapshot.line_strength = sample.line_strength;
        g_grayscale_snapshot.line_error = sample.line_error;
        g_grayscale_snapshot.sequence = sample.sequence;
        ++g_grayscale_publish_sequence;
        vTaskDelayUntil(&last_wake_time, interval);
    }
}

#if GRAY_VOFA_TELEMETRY_ENABLE
static void gray_vofa_task(void *argument)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(GRAY_VOFA_TELEMETRY_INTERVAL_MS);
    board_grayscale_snapshot_t snapshot;
    float channels[GRAY_VOFA_CHANNEL_COUNT];
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(GRAY_VOFA_CHANNEL_COUNT)];
    uint32_t channel;

    (void)argument;
    for (;;) {
        grayscale_snapshot_copy(&snapshot);
        for (channel = 0U; channel < BOARD_GRAYSCALE_CHANNEL_COUNT; ++channel) {
            channels[channel] = (float)snapshot.raw[channel];
            channels[BOARD_GRAYSCALE_CHANNEL_COUNT + channel] =
                (float)snapshot.normalized[channel];
        }
        channels[16U] = (float)snapshot.digital;
        channels[17U] = (float)snapshot.black_mask;
        channels[18U] = (float)snapshot.black_count;
        channels[19U] = (float)snapshot.line_error;
        channels[20U] = (float)snapshot.line_strength;
        channels[21U] = (float)snapshot.sequence;
        if (vofa_justfloat_encode(frame, sizeof(frame), channels,
                                  GRAY_VOFA_CHANNEL_COUNT)) {
            board_uart_write(frame, sizeof(frame));
        }
        vTaskDelayUntil(&last_wake_time, interval);
    }
}
#endif
#if BUZZER_FEATURE_ENABLE
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

void vApplicationMallocFailedHook(void) { taskDISABLE_INTERRUPTS(); for (;;) { } }
void vApplicationGetIdleTaskMemory(StaticTask_t **task_buffer, StackType_t **stack_buffer, uint32_t *stack_size)
{ *task_buffer = &g_idle_task_buffer; *stack_buffer = g_idle_task_stack; *stack_size = configIDLE_TASK_STACK_DEPTH; }
void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{ (void)task; (void)task_name; taskDISABLE_INTERRUPTS(); for (;;) { } }

int main(void)
{
    QueueHandle_t uart_queue;
    static const uint16_t grayscale_white[BOARD_GRAYSCALE_CHANNEL_COUNT] =
        {2834U, 3064U, 2150U, 1924U, 3099U, 3032U, 3182U, 2467U};
    static const uint16_t grayscale_black[BOARD_GRAYSCALE_CHANNEL_COUNT] =
        {353U, 1075U, 139U, 189U, 1027U, 593U, 2033U, 110U};

    SYSCFG_DL_init();
#if BUTTON_FEATURE_ENABLE
    board_buttons_init();
#endif
    board_encoder_init();
    board_grayscale_init(grayscale_white, grayscale_black);
    line_tracking_init(&g_line_tracking_state, 0);
    motor_control_init();
    NVIC_EnableIRQ(GPIOA_INT_IRQn);
    board_buzzer_init(BUZZER_FREQUENCY_HZ, BUZZER_DUTY_PERCENT);
    uart_queue = xQueueCreateStatic(UART_RX_QUEUE_LENGTH, sizeof(uint8_t), g_uart_queue_storage, &g_uart_queue_buffer);
    configASSERT(uart_queue != NULL);
    board_uart_enable_rx_interrupt(uart_queue);
    configASSERT(xTaskCreateStatic(motor_task, "motor", MOTOR_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_motor_task_stack, &g_motor_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(ws2812_task, "ws2812", WS2812_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_ws2812_task_stack, &g_ws2812_task_buffer) != NULL);
#if BUTTON_FEATURE_ENABLE
    configASSERT(xTaskCreateStatic(button_task, "buttons", BUTTON_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_button_task_stack, &g_button_task_buffer) != NULL);
#endif
    configASSERT(xTaskCreateStatic(board_uart_tx_task, "uart_tx", UART_TX_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_uart_tx_task_stack, &g_uart_tx_task_buffer) != NULL);
#if CRSF_REMOTE_CONTROL_ENABLE
    configASSERT(xTaskCreateStatic(crsf_task, "crsf", CRSF_TASK_STACK_DEPTH,
                                   NULL, APP_TASK_PRIORITY,
                                   g_crsf_task_stack, &g_crsf_task_buffer) != NULL);
#endif
#if !VOFA_SPEED_PID_TELEMETRY_ENABLE && !GRAY_VOFA_TELEMETRY_ENABLE && !IMU_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(uart_echo_task, "uart", UART_TASK_STACK_DEPTH, uart_queue, APP_TASK_PRIORITY, g_uart_task_stack, &g_uart_task_buffer) != NULL);
#endif
#if VOFA_SPEED_PID_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(telemetry_task, "telemetry", VOFA_SPEED_PID_TELEMETRY_TASK_STACK_DEPTH, NULL, VOFA_SPEED_PID_TELEMETRY_TASK_PRIORITY, g_telemetry_task_stack, &g_telemetry_task_buffer) != NULL);
#endif
    configASSERT(xTaskCreateStatic(imu_task, "imu", IMU_TASK_STACK_DEPTH, NULL, IMU_TASK_PRIORITY, g_imu_task_stack, &g_imu_task_buffer) != NULL);
    configASSERT(xTaskCreateStatic(gray_task, "gray", GRAY_TASK_STACK_DEPTH, NULL, TELEMETRY_TASK_PRIORITY, g_gray_task_stack, &g_gray_task_buffer) != NULL);
#if GRAY_VOFA_TELEMETRY_ENABLE
    configASSERT(xTaskCreateStatic(gray_vofa_task, "gray_vofa", GRAY_TASK_STACK_DEPTH, NULL, TELEMETRY_TASK_PRIORITY, g_gray_telemetry_task_stack, &g_gray_telemetry_task_buffer) != NULL);
#endif
#if BUZZER_FEATURE_ENABLE
    configASSERT(xTaskCreateStatic(buzzer_task, "buzzer", BUZZER_TASK_STACK_DEPTH, NULL, APP_TASK_PRIORITY, g_buzzer_task_stack, &g_buzzer_task_buffer) != NULL);
#endif
    vTaskStartScheduler();
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}
