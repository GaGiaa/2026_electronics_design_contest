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

#endif
