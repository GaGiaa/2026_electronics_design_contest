#include <assert.h>
#include <stdint.h>

#include "ti_msp_dl_config.h"
#include "drivers/grayscale/board_grayscale.h"

GPIO_Regs g_test_gray_gpio;
ADC12_Regs g_test_adc;
uint16_t g_test_adc_value;

static void test_calibration_mirror_and_private_values(void)
{
    const uint16_t white[BOARD_GRAYSCALE_CHANNEL_COUNT] = {
        100U, 100U, 100U, 100U, 100U, 100U, 100U, 100U
    };
    const uint16_t black[BOARD_GRAYSCALE_CHANNEL_COUNT] = {
        400U, 400U, 400U, 400U, 400U, 400U, 400U, 400U
    };
    board_grayscale_snapshot_t snapshot = {0};

    board_grayscale_init(white, black);
    assert(g_grayscale_debug.white[0] == 400U);
    assert(g_grayscale_debug.black[0] == 100U);
    assert(g_grayscale_debug.gray_white[0] == 300U);
    assert(g_grayscale_debug.gray_black[0] == 200U);
    assert(g_grayscale_debug.digital == 0U);
    assert(g_grayscale_debug.sequence == 0U);

    g_grayscale_debug.white[0] = 4095U;
    g_test_adc_value = 250U;
    board_grayscale_sample(&snapshot);

    assert(snapshot.sequence == 1U);
    assert(g_grayscale_debug.sequence == 1U);
    assert(snapshot.normalized[0] == 2047U);
    assert(g_grayscale_debug.digital == 0U);
}

static void test_digital_state_is_mirrored_after_sampling(void)
{
    board_grayscale_snapshot_t snapshot = {0};

    g_test_adc_value = 500U;
    board_grayscale_sample(&snapshot);

    assert(snapshot.sequence == 2U);
    assert(snapshot.digital == 0xFFU);
    assert(g_grayscale_debug.digital == 0xFFU);
    assert(g_grayscale_debug.sequence == 2U);
}

int main(void)
{
    test_calibration_mirror_and_private_values();
    test_digital_state_is_mirrored_after_sampling();
    return 0;
}
