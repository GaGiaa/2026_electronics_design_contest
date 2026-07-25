#ifndef TEST_TI_MSP_DL_CONFIG_H
#define TEST_TI_MSP_DL_CONFIG_H

#include <stdint.h>

typedef struct {
    uint32_t pins;
} GPIO_Regs;

typedef struct {
    uint32_t reserved;
} ADC12_Regs;

extern GPIO_Regs g_test_gray_gpio;
extern ADC12_Regs g_test_adc;
extern uint16_t g_test_adc_value;

#define GRAY_ADDRESS_PORT (&g_test_gray_gpio)
#define GRAY_ADDRESS_AD0_PIN 0x01U
#define GRAY_ADDRESS_AD1_PIN 0x02U
#define GRAY_ADDRESS_AD2_PIN 0x04U
#define GRAYSCALE_ADC_INST (&g_test_adc)
#define GRAYSCALE_ADC_ADCMEM_ADC_CH0 0U

#define DL_ADC12_REPEAT_MODE_DISABLED 0U
#define DL_ADC12_SAMPLING_SOURCE_AUTO 0U
#define DL_ADC12_TRIG_SRC_SOFTWARE 0U
#define DL_ADC12_SAMP_CONV_RES_12_BIT 0U
#define DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED 0U
#define DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED 0U

static inline void DL_GPIO_setPins(GPIO_Regs *port, uint32_t pins)
{
    (void)port;
    (void)pins;
}

static inline void DL_GPIO_clearPins(GPIO_Regs *port, uint32_t pins)
{
    (void)port;
    (void)pins;
}

static inline void DL_Common_delayCycles(uint32_t cycles)
{
    (void)cycles;
}

static inline void DL_ADC12_disableConversions(ADC12_Regs *adc)
{
    (void)adc;
}

static inline void DL_ADC12_initSingleSample(
    ADC12_Regs *adc, uint32_t repeat_mode, uint32_t sampling_source,
    uint32_t trigger_source, uint32_t resolution, uint32_t data_format)
{
    (void)adc;
    (void)repeat_mode;
    (void)sampling_source;
    (void)trigger_source;
    (void)resolution;
    (void)data_format;
}

static inline void DL_ADC12_enableConversions(ADC12_Regs *adc)
{
    (void)adc;
}

static inline void DL_ADC12_clearInterruptStatus(ADC12_Regs *adc,
                                                  uint32_t status)
{
    (void)adc;
    (void)status;
}

static inline void DL_ADC12_startConversion(ADC12_Regs *adc)
{
    (void)adc;
}

static inline uint32_t DL_ADC12_getRawInterruptStatus(ADC12_Regs *adc,
                                                       uint32_t status)
{
    (void)adc;
    (void)status;
    return 1U;
}

static inline uint16_t DL_ADC12_getMemResult(ADC12_Regs *adc,
                                              uint32_t memory_index)
{
    (void)adc;
    (void)memory_index;
    return g_test_adc_value;
}

#endif
