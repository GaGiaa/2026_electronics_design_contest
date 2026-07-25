#include "board_buzzer.h"

#include "ti_msp_dl_config.h"

static uint32_t g_buzzer_compare;

void board_buzzer_init(uint32_t frequency_hz, uint8_t duty_percent)
{
    uint32_t period;

    if ((frequency_hz < 1000U) || (frequency_hz > 20000U)) {
        g_buzzer_compare = 0U;
        board_buzzer_stop();
        return;
    }

    if (duty_percent > 100U) {
        duty_percent = 100U;
    }

    period = BUZZER_INST_CLK_FREQ / frequency_hz;
    DL_Timer_setLoadValue(BUZZER_INST, period - 1U);
    g_buzzer_compare = (period * duty_percent) / 100U;
    board_buzzer_stop();
}

void board_buzzer_start(void)
{
    DL_Timer_setCaptureCompareValue(BUZZER_INST, g_buzzer_compare,
                                    GPIO_BUZZER_C1_IDX);
}

void board_buzzer_stop(void)
{
    DL_Timer_setCaptureCompareValue(BUZZER_INST, 0U, GPIO_BUZZER_C1_IDX);
}
