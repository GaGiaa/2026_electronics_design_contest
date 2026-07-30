#include <assert.h>
#include <stdint.h>

#include "gpio.h"
#include "app_buttons.h"

GPIO_TypeDef g_test_gpio_a = {1U};
GPIO_TypeDef g_test_gpio_c = {2U};
uint32_t g_test_gpio_a_levels;
uint32_t g_test_gpio_c_levels;

static h723_app_buttons_snapshot_t snapshot(void)
{
    h723_app_buttons_snapshot_t value;

    h723_app_buttons_snapshot_copy(&value);
    return value;
}

static void test_initial_state_is_released(void)
{
    h723_app_buttons_snapshot_t value;

    g_test_gpio_a_levels = 0U;
    g_test_gpio_c_levels = 0U;
    h723_app_buttons_init();
    value = snapshot();

    assert(value.raw_high_mask == 0U);
    assert(value.stable_high_mask == 0U);
    assert(value.sample_sequence == 0U);
}

static void test_high_level_requires_two_samples(void)
{
    h723_app_buttons_snapshot_t value;

    g_test_gpio_c_levels = GPIO_PIN_5;
    h723_app_buttons_step();
    value = snapshot();
    assert(value.raw_high_mask == (1U << H723_APP_BUTTON_PC5));
    assert(value.stable_high_mask == 0U);
    assert(value.sample_sequence == 1U);

    h723_app_buttons_step();
    value = snapshot();
    assert(value.stable_high_mask == (1U << H723_APP_BUTTON_PC5));
    assert(value.sample_sequence == 2U);
}

static void test_low_level_requires_two_samples_to_release(void)
{
    h723_app_buttons_snapshot_t value;

    g_test_gpio_c_levels = 0U;
    h723_app_buttons_step();
    value = snapshot();
    assert(value.stable_high_mask == (1U << H723_APP_BUTTON_PC5));

    h723_app_buttons_step();
    value = snapshot();
    assert(value.stable_high_mask == 0U);
}

static void test_pin_order_maps_pc4_and_pa6(void)
{
    h723_app_buttons_snapshot_t value;

    g_test_gpio_c_levels = GPIO_PIN_4;
    g_test_gpio_a_levels = GPIO_PIN_6;
    h723_app_buttons_step();
    h723_app_buttons_step();
    value = snapshot();

    assert(value.raw_high_mask == ((1U << H723_APP_BUTTON_PC4) |
                                   (1U << H723_APP_BUTTON_PA6)));
    assert(value.stable_high_mask == ((1U << H723_APP_BUTTON_PC4) |
                                      (1U << H723_APP_BUTTON_PA6)));
}

int main(void)
{
    test_initial_state_is_released();
    test_high_level_requires_two_samples();
    test_low_level_requires_two_samples_to_release();
    test_pin_order_maps_pc4_and_pa6();
    return 0;
}
