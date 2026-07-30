#include "app_buzzer.h"

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"
#include "app_time.h"
#include "tim.h"

#if (APP_H723_BUZZER_TEST_ENABLE == 1U)
#define APP_H723_BUZZER_TIMER_CLOCK_HZ 1000000U

static bool g_buzzer_on;
static uint32_t g_buzzer_phase_start_ms;

static uint32_t buzzer_period_counts(void)
{
    return APP_H723_BUZZER_TIMER_CLOCK_HZ / APP_H723_BUZZER_FREQUENCY_HZ;
}

static uint32_t buzzer_on_counts(void)
{
    return (buzzer_period_counts() * APP_H723_BUZZER_DUTY_PERCENT) / 100U;
}

static void buzzer_set_output(bool enabled)
{
    uint32_t compare = enabled ? buzzer_on_counts() : 0U;

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, compare);
}
#endif

void h723_buzzer_service_init(void)
{
#if (APP_H723_BUZZER_TEST_ENABLE == 1U)
    uint32_t period = buzzer_period_counts();

    __HAL_TIM_SET_AUTORELOAD(&htim2, period - 1U);
    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    g_buzzer_on = true;
    g_buzzer_phase_start_ms = h723_app_time_now_ms();
    buzzer_set_output(true);
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4) != HAL_OK) {
        buzzer_set_output(false);
        g_buzzer_on = false;
    }
#endif
}

void h723_buzzer_service_step(uint32_t now_ms)
{
#if (APP_H723_BUZZER_TEST_ENABLE == 1U)
    uint32_t phase_duration_ms = g_buzzer_on
                                     ? APP_H723_BUZZER_ON_TIME_MS
                                     : APP_H723_BUZZER_OFF_TIME_MS;

    if ((uint32_t)(now_ms - g_buzzer_phase_start_ms) >= phase_duration_ms) {
        g_buzzer_on = !g_buzzer_on;
        g_buzzer_phase_start_ms = now_ms;
        buzzer_set_output(g_buzzer_on);
    }
#else
    (void)now_ms;
#endif
}
