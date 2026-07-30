#ifndef APP_BUTTONS_H
#define APP_BUTTONS_H

#include <stdint.h>

#define H723_APP_BUTTON_COUNT 3U

typedef enum {
    H723_APP_BUTTON_PC5 = 0U,
    H723_APP_BUTTON_PC4,
    H723_APP_BUTTON_PA6
} h723_app_button_id_t;

typedef struct {
    uint32_t raw_high_mask;
    uint32_t stable_high_mask;
    uint32_t sample_sequence;
} h723_app_buttons_snapshot_t;

/**
 * @brief 初始化三个高电平有效按键的采样状态。
 *
 * 当前 GPIO 电平会作为初始稳定状态，不产生上电瞬间的虚假变化。
 */
void h723_app_buttons_init(void);

/**
 * @brief 执行一次按键采样和消抖。
 *
 * 该函数只能在任务上下文调用，调用周期由
 * APP_H723_BUTTON_TASK_PERIOD_MS 约束。
 */
void h723_app_buttons_step(void);

/**
 * @brief 复制当前按键快照。
 *
 * @param[out] snapshot 输出原始电平、稳定电平和采样序号。
 */
void h723_app_buttons_snapshot_copy(h723_app_buttons_snapshot_t *snapshot);

#endif
