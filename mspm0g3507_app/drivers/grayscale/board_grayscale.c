#include "board_grayscale.h"

#include <stdbool.h>
#include <stddef.h>

#include "ti_msp_dl_config.h"

#define GRAYSCALE_ADC_SAMPLES_PER_CHANNEL 8U
#define GRAYSCALE_ADDRESS_SETTLE_CYCLES 80U
#define GRAYSCALE_ADC_MAX_VALUE 4095U
#define GRAYSCALE_ADC_WAIT_LIMIT 100000U

/* Wiring: AD0=PB13, AD1=PB1, AD2=PB23, OUT=PA27. */
static uint16_t g_white[BOARD_GRAYSCALE_CHANNEL_COUNT];
static uint16_t g_black[BOARD_GRAYSCALE_CHANNEL_COUNT];
static uint16_t g_gray_white[BOARD_GRAYSCALE_CHANNEL_COUNT];
static uint16_t g_gray_black[BOARD_GRAYSCALE_CHANNEL_COUNT];
static uint8_t g_digital;
static uint32_t g_sequence;
volatile board_grayscale_debug_state_t g_grayscale_debug;
volatile uint32_t g_grayscale_adc_timeout_count;

static uint16_t clamp_adc_value(uint32_t value)
{
    return (value > GRAYSCALE_ADC_MAX_VALUE) ?
        GRAYSCALE_ADC_MAX_VALUE : (uint16_t)value;
}

static void set_address(uint8_t channel)
{
    if ((channel & 0x01U) != 0U) {
        DL_GPIO_setPins(GRAY_ADDRESS_PORT, GRAY_ADDRESS_AD0_PIN);
    } else {
        DL_GPIO_clearPins(GRAY_ADDRESS_PORT, GRAY_ADDRESS_AD0_PIN);
    }
    if ((channel & 0x02U) != 0U) {
        DL_GPIO_setPins(GRAY_ADDRESS_PORT, GRAY_ADDRESS_AD1_PIN);
    } else {
        DL_GPIO_clearPins(GRAY_ADDRESS_PORT, GRAY_ADDRESS_AD1_PIN);
    }
    if ((channel & 0x04U) != 0U) {
        DL_GPIO_setPins(GRAY_ADDRESS_PORT, GRAY_ADDRESS_AD2_PIN);
    } else {
        DL_GPIO_clearPins(GRAY_ADDRESS_PORT, GRAY_ADDRESS_AD2_PIN);
    }
    DL_Common_delayCycles(GRAYSCALE_ADDRESS_SETTLE_CYCLES);
}

static uint16_t read_average(bool *timed_out)
{
    uint32_t total = 0U;
    uint32_t sample;

    if (timed_out != NULL) {
        *timed_out = false;
    }

    for (sample = 0U; sample < GRAYSCALE_ADC_SAMPLES_PER_CHANNEL; ++sample) {
        uint32_t wait_count = 0U;

        DL_ADC12_clearInterruptStatus(GRAYSCALE_ADC_INST,
                                       DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
        DL_ADC12_startConversion(GRAYSCALE_ADC_INST);
        while ((DL_ADC12_getRawInterruptStatus(
                    GRAYSCALE_ADC_INST,
                    DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED) == 0U) &&
               (wait_count < GRAYSCALE_ADC_WAIT_LIMIT)) {
            ++wait_count;
        }
        if (wait_count == GRAYSCALE_ADC_WAIT_LIMIT) {
            ++g_grayscale_adc_timeout_count;
            if (timed_out != NULL) {
                *timed_out = true;
            }
            DL_ADC12_enableConversions(GRAYSCALE_ADC_INST);
            return 0U;
        }
        total += DL_ADC12_getMemResult(GRAYSCALE_ADC_INST,
                                       GRAYSCALE_ADC_ADCMEM_ADC_CH0);
        DL_ADC12_enableConversions(GRAYSCALE_ADC_INST);
    }
    return (uint16_t)(total / GRAYSCALE_ADC_SAMPLES_PER_CHANNEL);
}

static uint16_t normalize(uint16_t value, uint16_t black, uint16_t white)
{
    uint32_t scaled;

    if (white <= black) {
        return 0U;
    }
    if (value <= black) {
        return 0U;
    }
    if (value >= white) {
        return GRAYSCALE_ADC_MAX_VALUE;
    }
    scaled = ((uint32_t)(value - black) * GRAYSCALE_ADC_MAX_VALUE) /
             (uint32_t)(white - black);
    return clamp_adc_value(scaled);
}

void board_grayscale_init(const uint16_t *white, const uint16_t *black)
{
    uint32_t channel;

    /* SysConfig configures the ADC memory, but some Keil generations omit the
     * ADC control-mode initializer. Reapply the complete single-sample mode
     * here so the software-triggered conversion cannot remain stuck. */
    DL_ADC12_disableConversions(GRAYSCALE_ADC_INST);
    DL_ADC12_initSingleSample(GRAYSCALE_ADC_INST,
                              DL_ADC12_REPEAT_MODE_DISABLED,
                              DL_ADC12_SAMPLING_SOURCE_AUTO,
                              DL_ADC12_TRIG_SRC_SOFTWARE,
                              DL_ADC12_SAMP_CONV_RES_12_BIT,
                              DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_enableConversions(GRAYSCALE_ADC_INST);

    g_digital = 0U;
    g_sequence = 0U;
    g_grayscale_debug = (board_grayscale_debug_state_t){0};
    g_grayscale_adc_timeout_count = 0U;
    for (channel = 0U; channel < BOARD_GRAYSCALE_CHANNEL_COUNT; ++channel) {
        uint16_t calibrated_white = white[channel];
        uint16_t calibrated_black = black[channel];

        if (calibrated_white < calibrated_black) {
            uint16_t temporary = calibrated_white;
            calibrated_white = calibrated_black;
            calibrated_black = temporary;
        }
        g_white[channel] = calibrated_white;
        g_black[channel] = calibrated_black;
        g_gray_white[channel] = (uint16_t)((calibrated_black +
                                            (2U * calibrated_white)) / 3U);
        g_gray_black[channel] = (uint16_t)(((2U * calibrated_black) +
                                            calibrated_white) / 3U);
        g_grayscale_debug.white[channel] = calibrated_white;
        g_grayscale_debug.black[channel] = calibrated_black;
        g_grayscale_debug.gray_white[channel] = g_gray_white[channel];
        g_grayscale_debug.gray_black[channel] = g_gray_black[channel];
    }
}

void board_grayscale_sample(board_grayscale_snapshot_t *snapshot)
{
    uint32_t channel;

    if (snapshot == NULL) {
        return;
    }
    snapshot->adc_timeout_mask = 0U;
    for (channel = 0U; channel < BOARD_GRAYSCALE_CHANNEL_COUNT; ++channel) {
        uint16_t value;
        bool timed_out;

        set_address((uint8_t)channel);
        value = read_average(&timed_out);
        if (timed_out) {
            snapshot->adc_timeout_mask |= (uint8_t)(1U << channel);
        }
        snapshot->raw[channel] = value;
        snapshot->normalized[channel] = normalize(value, g_black[channel],
                                                   g_white[channel]);
        if (g_white[channel] <= g_black[channel]) {
            continue;
        }
        if (value >= g_gray_white[channel]) {
            g_digital |= (uint8_t)(1U << channel);
        } else if (value <= g_gray_black[channel]) {
            g_digital &= (uint8_t)~(1U << channel);
        }
    }
    snapshot->digital = g_digital;
    snapshot->sequence = ++g_sequence;
    g_grayscale_debug.digital = g_digital;
    g_grayscale_debug.sequence = g_sequence;
}
