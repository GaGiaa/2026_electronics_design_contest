#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_task_menu.h"

typedef struct {
    uint32_t clear_count;
    uint32_t draw_count;
    uint32_t last_page;
    uint32_t last_task;
    bool last_confirmed;
    char last_title[64];
} test_display_state_t;

static void test_display_clear(void *context)
{
    test_display_state_t *state = (test_display_state_t *)context;

    state->clear_count++;
}

static void test_display_draw_page(void *context, uint32_t page,
                                   uint32_t task, const char *title,
                                   bool confirmed)
{
    test_display_state_t *state = (test_display_state_t *)context;

    state->draw_count++;
    state->last_page = page;
    state->last_task = task;
    state->last_confirmed = confirmed;
    (void)strncpy(state->last_title, title, sizeof(state->last_title) - 1U);
    state->last_title[sizeof(state->last_title) - 1U] = '\0';
}

static app_task_menu_display_t make_display(test_display_state_t *state)
{
    const app_task_menu_display_t display = {
        .context = state,
        .clear = test_display_clear,
        .draw_page = test_display_draw_page,
    };

    return display;
}

static void test_menu_wraps_and_reports_selected_task(void)
{
    test_display_state_t display_state = {0};
    const app_task_menu_display_t display = make_display(&display_state);

    app_task_menu_init(&display);
    assert(app_task_menu_current_page() == 0U);
    assert(app_task_menu_selected_task() == 2U);
    assert(!app_task_menu_execution_requested());

    app_task_menu_key_event(APP_TASK_MENU_KEY_PREV, 200U);
    assert(app_task_menu_current_page() == 4U);
    assert(app_task_menu_selected_task() == 6U);
    assert(display_state.last_task == 6U);
}

static void test_menu_debounces_and_locks_after_confirm(void)
{
    test_display_state_t display_state = {0};
    const app_task_menu_display_t display = make_display(&display_state);

    app_task_menu_init(&display);
    app_task_menu_key_event(APP_TASK_MENU_KEY_NEXT, 200U);
    app_task_menu_key_event(APP_TASK_MENU_KEY_NEXT, 250U);
    assert(app_task_menu_current_page() == 1U);

    app_task_menu_key_event(APP_TASK_MENU_KEY_CONFIRM, 400U);
    assert(app_task_menu_execution_requested());
    assert(app_task_menu_selected_task() == 3U);
    assert(display_state.last_confirmed);

    app_task_menu_key_event(APP_TASK_MENU_KEY_NEXT, 600U);
    assert(app_task_menu_current_page() == 1U);

    app_task_menu_reset();
    assert(!app_task_menu_execution_requested());
    assert(app_task_menu_selected_task() == 2U);
}

int main(void)
{
    test_menu_wraps_and_reports_selected_task();
    test_menu_debounces_and_locks_after_confirm();
    return 0;
}
