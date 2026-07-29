#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "vofa_justfloat.h"

#define H723_HEALTH_CHANNEL_COUNT 6U

static void test_encode_health_frame_uses_expected_channel_layout(void)
{
    const float channels[H723_HEALTH_CHANNEL_COUNT] = {
        723.0f, 1000.0f, 20.0f, 3.0f, 2.0f, 1.0f
    };
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(H723_HEALTH_CHANNEL_COUNT)] = {0U};

    assert(vofa_justfloat_encode(frame, sizeof(frame), channels,
                                  H723_HEALTH_CHANNEL_COUNT));
    assert(frame[0U] == 0x00U);
    assert(frame[1U] == 0xC0U);
    assert(frame[2U] == 0x34U);
    assert(frame[3U] == 0x44U);
    assert(frame[4U] == 0x00U);
    assert(frame[5U] == 0x00U);
    assert(frame[6U] == 0x7AU);
    assert(frame[7U] == 0x44U);
    assert(frame[sizeof(frame) - 4U] == 0x00U);
    assert(frame[sizeof(frame) - 3U] == 0x00U);
    assert(frame[sizeof(frame) - 2U] == 0x80U);
    assert(frame[sizeof(frame) - 1U] == 0x7FU);
}

static void test_encode_health_frame_rejects_invalid_arguments(void)
{
    const float channels[H723_HEALTH_CHANNEL_COUNT] = {0.0f};
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(H723_HEALTH_CHANNEL_COUNT)] = {0U};

    assert(!vofa_justfloat_encode(NULL, sizeof(frame), channels,
                                  H723_HEALTH_CHANNEL_COUNT));
    assert(!vofa_justfloat_encode(frame, sizeof(frame) - 1U, channels,
                                  H723_HEALTH_CHANNEL_COUNT));
    assert(!vofa_justfloat_encode(frame, sizeof(frame), NULL,
                                  H723_HEALTH_CHANNEL_COUNT));
}

int main(void)
{
    test_encode_health_frame_uses_expected_channel_layout();
    test_encode_health_frame_rejects_invalid_arguments();
    return 0;
}
