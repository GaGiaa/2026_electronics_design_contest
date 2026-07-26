#include "board_buttons.h"

#include <stdbool.h>

#include "ti_msp_dl_config.h"

typedef struct {
    GPIO_Regs *port;
    uint32_t pin;
} board_button_pin_t;

static const board_button_pin_t g_button_pins[BOARD_BUTTON_COUNT] = {
    {BUTTONS_BUTTON_PA7_PORT, BUTTONS_BUTTON_PA7_PIN},
    {BUTTONS_BUTTON_PB12_PORT, BUTTONS_BUTTON_PB12_PIN},
    {BUTTONS_BUTTON_PA8_PORT, BUTTONS_BUTTON_PA8_PIN},
    {BUTTONS_BUTTON_PA30_PORT, BUTTONS_BUTTON_PA30_PIN}
};

static bool g_stable_pressed[BOARD_BUTTON_COUNT];
static bool g_candidate_pressed[BOARD_BUTTON_COUNT];
static uint8_t g_candidate_samples[BOARD_BUTTON_COUNT];

static bool button_is_pressed(board_button_t button)
{
    return DL_GPIO_readPins(g_button_pins[button].port,
                            g_button_pins[button].pin) == 0U;
}

void board_buttons_init(void)
{
    uint32_t button;

    for (button = 0U; button < BOARD_BUTTON_COUNT; ++button) {
        g_stable_pressed[button] = button_is_pressed((board_button_t)button);
        g_candidate_pressed[button] = g_stable_pressed[button];
        g_candidate_samples[button] = BOARD_BUTTON_DEBOUNCE_SAMPLES;
    }
}

board_buttons_events_t board_buttons_scan(void)
{
    board_buttons_events_t events = {0U, 0U};
    uint32_t button;

    for (button = 0U; button < BOARD_BUTTON_COUNT; ++button) {
        bool pressed = button_is_pressed((board_button_t)button);

        if (pressed != g_candidate_pressed[button]) {
            g_candidate_pressed[button] = pressed;
            g_candidate_samples[button] = 1U;
        } else if (g_candidate_samples[button] < BOARD_BUTTON_DEBOUNCE_SAMPLES) {
            ++g_candidate_samples[button];
        }

        if ((g_candidate_samples[button] >= BOARD_BUTTON_DEBOUNCE_SAMPLES) &&
            (g_stable_pressed[button] != g_candidate_pressed[button])) {
            g_stable_pressed[button] = g_candidate_pressed[button];
            if (g_stable_pressed[button]) {
                events.pressed_mask |= (1U << button);
            } else {
                events.released_mask |= (1U << button);
            }
        }
        if (g_stable_pressed[button]) {
            events.stable_pressed_mask |= (1U << button);
        }
    }

    return events;
}
