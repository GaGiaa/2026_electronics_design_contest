#include "app_grayscale_service.h"

#include "app_config.h"
#include "app_debug.h"
#include "app_grayscale.h"

static const uint16_t s_grayscale_white[H723_GRAYSCALE_CHANNEL_COUNT] =
    {580U, 1730U, 730U, 370U, 2100U, 1580U, 3100U, 500U};
static const uint16_t s_grayscale_black[H723_GRAYSCALE_CHANNEL_COUNT] =
    {470U, 690U, 400U, 285U, 750U, 390U, 1300U, 300U};
static h723_grayscale_snapshot_t s_snapshot;

void h723_grayscale_service_init(void)
{
    h723_grayscale_init(s_grayscale_white, s_grayscale_black);
}

void h723_grayscale_service_step(uint32_t now_ms)
{
    uint32_t channel;

    (void)now_ms;
    if (!h723_grayscale_sample(&s_snapshot)) {
        g_h723_debug.grayscale.adc_timeout_mask = s_snapshot.adc_timeout_mask;
    }
    for (channel = 0U; channel < H723_GRAYSCALE_CHANNEL_COUNT; ++channel) {
        g_h723_debug.grayscale.raw[channel] = s_snapshot.raw[channel];
        g_h723_debug.grayscale.normalized[channel] =
            s_snapshot.normalized[channel];
    }
    g_h723_debug.grayscale.digital = s_snapshot.digital;
    g_h723_debug.grayscale.black_mask = s_snapshot.black_mask;
    g_h723_debug.grayscale.adc_timeout_mask = s_snapshot.adc_timeout_mask;
    g_h723_debug.grayscale.black_count = s_snapshot.black_count;
    g_h723_debug.grayscale.line_strength = s_snapshot.line_strength;
    g_h723_debug.grayscale.line_error = s_snapshot.line_error;
    g_h723_debug.grayscale.sequence = s_snapshot.sequence;
    g_h723_debug.grayscale.adc_timeout_count = s_snapshot.adc_timeout_count;
}
