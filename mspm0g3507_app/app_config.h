#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* 手动调试时只修改这里；构建参数可覆盖这个默认值。 */
#ifndef RTOS_MONITOR_ENABLE
#define RTOS_MONITOR_ENABLE 0U
#endif

/* SSD1306 OLED 验收任务开关；构建参数可覆盖这个默认值。 */
#ifndef OLED_TEST_TASK_ENABLE
#define OLED_TEST_TASK_ENABLE 0U
#endif

/* 常见 SSD1306 I2C 7-bit 地址；少数模块可改为 0x3DU。 */
#ifndef OLED_I2C_ADDRESS
#define OLED_I2C_ADDRESS 0x3CU
#endif

/* PB8 舵机调试任务；默认关闭，避免改变默认应用行为。 */
#ifndef SERVO_FEATURE_ENABLE
#define SERVO_FEATURE_ENABLE 0U
#endif

/* 舵机角度和脉宽映射参数；可通过编译定义或本文件覆盖。 */
#ifndef SERVO_MAX_ANGLE_DEG
#define SERVO_MAX_ANGLE_DEG 180U
#endif
#ifndef SERVO_INITIAL_ANGLE_DEG
#define SERVO_INITIAL_ANGLE_DEG 90U
#endif
#ifndef SERVO_MIN_PULSE_US
#define SERVO_MIN_PULSE_US 500U
#endif
#ifndef SERVO_MAX_PULSE_US
#define SERVO_MAX_PULSE_US 2500U
#endif
#ifndef SERVO_TASK_INTERVAL_MS
#define SERVO_TASK_INTERVAL_MS 20U
#endif

#if (SERVO_MAX_ANGLE_DEG == 0U)
#error "SERVO_MAX_ANGLE_DEG must be nonzero"
#endif
#if (SERVO_MIN_PULSE_US >= SERVO_MAX_PULSE_US)
#error "SERVO_MIN_PULSE_US must be less than SERVO_MAX_PULSE_US"
#endif
#if (SERVO_INITIAL_ANGLE_DEG > SERVO_MAX_ANGLE_DEG)
#error "SERVO_INITIAL_ANGLE_DEG must not exceed SERVO_MAX_ANGLE_DEG"
#endif
#if (SERVO_MAX_PULSE_US >= 20000U)
#error "SERVO_MAX_PULSE_US must fit inside the 20 ms servo period"
#endif

#endif
