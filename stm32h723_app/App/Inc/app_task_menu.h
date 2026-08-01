#ifndef APP_TASK_MENU_H
#define APP_TASK_MENU_H

#include <stdbool.h>
#include <stdint.h>

#define APP_TASK_MENU_BALANCE_SETUP_ID 7U

typedef enum {
    APP_TASK_MENU_KEY_PREV = 0,
    APP_TASK_MENU_KEY_NEXT,
    APP_TASK_MENU_KEY_CONFIRM
} app_task_menu_key_t;

/**
 * @brief 任务菜单的显示适配器。
 *
 * 本模块不拥有显示器或按键驱动；调用方通过这些回调接入具体硬件。
 */
typedef struct {
    void *context;
    void (*clear)(void *context);
    void (*draw_page)(void *context, uint32_t page, uint32_t task,
                      const char *title, bool confirmed);
} app_task_menu_display_t;

/**
 * @brief 初始化任务 2 至任务 6 以及钢珠平衡点设定页的菜单。
 *
 * @param[in] display 可为空的显示适配器。
 */
void app_task_menu_init(const app_task_menu_display_t *display);

/**
 * @brief 处理已去硬件毛刺前的菜单按键事件。
 *
 * 模块使用 `now_ms` 实施 `APP_TASK_MENU_KEY_DEBOUNCE_MS` 软件去抖；确认后
 * 菜单锁定，直至调用 reset。
 *
 * @param[in] key 按键类型。
 * @param[in] now_ms 当前单调毫秒计数。
 */
void app_task_menu_key_event(app_task_menu_key_t key, uint32_t now_ms);

/**
 * @brief 恢复初始页面并解除确认锁定。
 */
void app_task_menu_reset(void);

/**
 * @brief 完成或拒绝当前任务并解除菜单锁定，保留当前选择页面。
 */
void app_task_menu_finish_execution(void);

/**
 * @brief 获取当前页面编号，范围为 0 至 `APP_TASK_MENU_PAGE_COUNT - 1`。
 */
uint32_t app_task_menu_current_page(void);

/**
 * @brief 获取当前页面或已确认页面对应的任务号。
 */
uint32_t app_task_menu_selected_task(void);

/**
 * @brief 查询是否已通过确认键请求执行所选任务。
 */
bool app_task_menu_execution_requested(void);

/**
 * @brief 取走一次已确认的任务请求。
 *
 * @param[out] task_id 输出任务号，不能为空。
 * @return 存在未消费请求并成功写出时返回 true。
 */
bool app_task_menu_take_execution_request(uint32_t *task_id);

#endif
