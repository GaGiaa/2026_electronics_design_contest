#include "app_task_menu.h"

#include <string.h>

#include "app_config.h"

static const char *const s_task_titles[APP_TASK_MENU_PAGE_COUNT] = {
    "Task 2: lap and stop at A",
    "Task 3: ball +5cm to -5cm",
    "Task 4: carry ball through B",
    "Task 5: lap and pass A",
    "Task 6: lap and stop ball"
};

static app_task_menu_display_t s_display;
static uint32_t s_current_page;
static uint32_t s_selected_task;
static bool s_execution_requested;
static bool s_locked;
static bool s_has_key_time;
static uint32_t s_last_key_ms;

static uint32_t app_task_menu_page_to_task(uint32_t page)
{
    return APP_TASK_MENU_FIRST_TASK_ID + page;
}

static void app_task_menu_render(void)
{
    if (s_display.clear != NULL) {
        s_display.clear(s_display.context);
    }
    if (s_display.draw_page != NULL) {
        s_display.draw_page(s_display.context, s_current_page,
                            app_task_menu_page_to_task(s_current_page),
                            s_task_titles[s_current_page], s_locked);
    }
}

void app_task_menu_init(const app_task_menu_display_t *display)
{
    (void)memset(&s_display, 0, sizeof(s_display));
    if (display != NULL) {
        s_display = *display;
    }

    s_current_page = 0U;
    s_selected_task = 0U;
    s_execution_requested = false;
    s_locked = false;
    s_has_key_time = false;
    s_last_key_ms = 0U;
    app_task_menu_render();
}

void app_task_menu_key_event(app_task_menu_key_t key, uint32_t now_ms)
{
    if (s_locked || key > APP_TASK_MENU_KEY_CONFIRM) {
        return;
    }
    if (s_has_key_time &&
        (uint32_t)(now_ms - s_last_key_ms) < APP_TASK_MENU_KEY_DEBOUNCE_MS) {
        return;
    }

    s_has_key_time = true;
    s_last_key_ms = now_ms;
    if (key == APP_TASK_MENU_KEY_PREV) {
        s_current_page = s_current_page == 0U ?
            APP_TASK_MENU_PAGE_COUNT - 1U : s_current_page - 1U;
        app_task_menu_render();
    } else if (key == APP_TASK_MENU_KEY_NEXT) {
        s_current_page = (s_current_page + 1U) % APP_TASK_MENU_PAGE_COUNT;
        app_task_menu_render();
    } else {
        s_selected_task = app_task_menu_page_to_task(s_current_page);
        s_execution_requested = true;
        s_locked = true;
        app_task_menu_render();
    }
}

void app_task_menu_reset(void)
{
    s_current_page = 0U;
    s_selected_task = 0U;
    s_execution_requested = false;
    s_locked = false;
    s_has_key_time = false;
    s_last_key_ms = 0U;
    app_task_menu_render();
}

uint32_t app_task_menu_current_page(void)
{
    return s_current_page;
}

uint32_t app_task_menu_selected_task(void)
{
    return s_execution_requested ? s_selected_task :
        app_task_menu_page_to_task(s_current_page);
}

bool app_task_menu_execution_requested(void)
{
    return s_execution_requested;
}

bool app_task_menu_take_execution_request(uint32_t *task_id)
{
    if (task_id == NULL || !s_execution_requested) {
        return false;
    }
    *task_id = s_selected_task;
    s_execution_requested = false;
    return true;
}
