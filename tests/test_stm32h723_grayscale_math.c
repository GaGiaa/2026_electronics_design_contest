#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "app_grayscale_math.h"

static void test_normalization_and_hysteresis(void)
{
    const uint16_t white[H723_GRAYSCALE_CHANNEL_COUNT] =
        {1000U, 1000U, 1000U, 1000U, 1000U, 1000U, 1000U, 1000U};
    const uint16_t black[H723_GRAYSCALE_CHANNEL_COUNT] =
        {100U, 100U, 100U, 100U, 100U, 100U, 100U, 100U};
    const uint16_t raw[H723_GRAYSCALE_CHANNEL_COUNT] =
        {100U, 400U, 700U, 1000U, 300U, 600U, 900U, 100U};
    h723_grayscale_derived_t derived = {0};

    h723_grayscale_derive(raw, white, black, 0U, 0, &derived);

    assert(derived.normalized[0] == 0U);
    assert(derived.normalized[1] == 1365U);
    assert(derived.normalized[2] == 2730U);
    assert(derived.normalized[3] == H723_GRAYSCALE_ADC_MAX);
    assert(derived.digital == 0x4CU);
    assert(derived.black_mask == 0xB3U);
    assert(derived.black_count == 5U);
    assert(derived.line_strength == 17745U);
    assert(derived.line_error == -192);
    assert(fabsf(derived.line_position + 0.192f) < 0.001f);
}

static void test_hysteresis_preserves_state_between_thresholds(void)
{
    const uint16_t white[H723_GRAYSCALE_CHANNEL_COUNT] =
        {1000U, 1000U, 1000U, 1000U, 1000U, 1000U, 1000U, 1000U};
    const uint16_t black[H723_GRAYSCALE_CHANNEL_COUNT] =
        {100U, 100U, 100U, 100U, 100U, 100U, 100U, 100U};
    uint16_t raw[H723_GRAYSCALE_CHANNEL_COUNT] = {0};
    h723_grayscale_derived_t derived = {0};

    raw[0] = 1000U;
    h723_grayscale_derive(raw, white, black, 0U, 0, &derived);
    assert((derived.digital & 0x01U) != 0U);

    raw[0] = 550U;
    h723_grayscale_derive(raw, white, black, derived.digital, 0, &derived);
    assert((derived.digital & 0x01U) != 0U);

    raw[0] = 100U;
    h723_grayscale_derive(raw, white, black, derived.digital, 0, &derived);
    assert((derived.digital & 0x01U) == 0U);
}

int main(void)
{
    test_normalization_and_hysteresis();
    test_hysteresis_preserves_state_between_thresholds();
    return 0;
}
