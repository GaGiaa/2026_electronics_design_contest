#include "app_buttons.h"

#include <stddef.h>

#include "app_config.h"
#include "gpio.h"

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} h723_app_button_pin_t;

static const h723_app_button_pin_t s_button_pins[H723_APP_BUTTON_COUNT] = {
    {GPIOC, GPIO_PIN_5},
    {GPIOC, GPIO_PIN_4},
    {GPIOA, GPIO_PIN_6}
};

static volatile uint32_t s_raw_high_mask;
static volatile uint32_t s_stable_high_mask;
static volatile uint32_t s_candidate_high_mask;
static volatile uint32_t s_candidate_samples;
static volatile uint32_t s_sample_sequence;

static uint32_t h723_app_buttons_read_mask(void)
{
    uint32_t mask = 0U;
    uint32_t button;

    for (button = 0U; button < H723_APP_BUTTON_COUNT; ++button) {
        if (HAL_GPIO_ReadPin(s_button_pins[button].port,
                             s_button_pins[button].pin) == GPIO_PIN_SET) {
            mask |= (1U << button);
        }
    }
    return mask;
}

void h723_app_buttons_init(void)
{
    s_raw_high_mask = h723_app_buttons_read_mask();
    s_stable_high_mask = s_raw_high_mask;
    s_candidate_high_mask = s_raw_high_mask;
    s_candidate_samples = APP_H723_BUTTON_DEBOUNCE_SAMPLES;
    s_sample_sequence = 0U;
}

void h723_app_buttons_step(void)
{
    s_raw_high_mask = h723_app_buttons_read_mask();
    ++s_sample_sequence;

    if (s_raw_high_mask != s_candidate_high_mask) {
        s_candidate_high_mask = s_raw_high_mask;
        s_candidate_samples = 1U;
    } else if (s_candidate_samples < APP_H723_BUTTON_DEBOUNCE_SAMPLES) {
        ++s_candidate_samples;
    }

    if ((s_candidate_samples >= APP_H723_BUTTON_DEBOUNCE_SAMPLES) &&
        (s_stable_high_mask != s_candidate_high_mask)) {
        s_stable_high_mask = s_candidate_high_mask;
    }
}

void h723_app_buttons_snapshot_copy(h723_app_buttons_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    snapshot->raw_high_mask = s_raw_high_mask;
    snapshot->stable_high_mask = s_stable_high_mask;
    snapshot->sample_sequence = s_sample_sequence;
}
