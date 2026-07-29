#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "vofa_justfloat.h"

#define H723_HEALTH_CHANNEL_COUNT 6U
#define H723_JY901S_CHANNEL_COUNT 10U
#define H723_SINGLE_MOTOR_CHANNEL_COUNT 8U

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

static void test_encode_jy901s_frame_keeps_ten_channel_order(void)
{
    const float channels[H723_JY901S_CHANNEL_COUNT] = {
        1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 25.5f
    };
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(H723_JY901S_CHANNEL_COUNT)] = {0U};

    assert(vofa_justfloat_encode(frame, sizeof(frame), channels, H723_JY901S_CHANNEL_COUNT));
    assert(frame[0U] == 0x00U && frame[1U] == 0x00U && frame[2U] == 0x80U && frame[3U] == 0x3FU);
    assert(frame[24U] == 0x00U && frame[25U] == 0x00U && frame[26U] == 0xE0U && frame[27U] == 0x40U);
    assert(frame[36U] == 0x00U && frame[37U] == 0x00U && frame[38U] == 0xCCU && frame[39U] == 0x41U);
    assert(frame[sizeof(frame) - 4U] == 0x00U);
    assert(frame[sizeof(frame) - 3U] == 0x00U);
    assert(frame[sizeof(frame) - 2U] == 0x80U);
    assert(frame[sizeof(frame) - 1U] == 0x7FU);
}

static void test_encode_single_motor_frame_keeps_eight_channel_order(void)
{
    const float channels[H723_SINGLE_MOTOR_CHANNEL_COUNT] = {
        100.0f, -20.0f, 1500.0f, -300.0f, 2500.0f, 1000.0f, 1200.0f, -50.0f
    };
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(H723_SINGLE_MOTOR_CHANNEL_COUNT)] = {0U};

    assert(vofa_justfloat_encode(frame, sizeof(frame), channels,
                                 H723_SINGLE_MOTOR_CHANNEL_COUNT));
    assert(frame[0U] == 0x00U && frame[1U] == 0x00U && frame[2U] == 0xC8U && frame[3U] == 0x42U);
    assert(frame[4U] == 0x00U && frame[5U] == 0x00U && frame[6U] == 0xA0U && frame[7U] == 0xC1U);
    assert(frame[8U] == 0x00U && frame[9U] == 0x80U && frame[10U] == 0xBBU && frame[11U] == 0x44U);
    assert(frame[28U] == 0x00U && frame[29U] == 0x00U && frame[30U] == 0x48U && frame[31U] == 0xC2U);
    assert(frame[sizeof(frame) - 4U] == 0x00U);
    assert(frame[sizeof(frame) - 3U] == 0x00U);
    assert(frame[sizeof(frame) - 2U] == 0x80U);
    assert(frame[sizeof(frame) - 1U] == 0x7FU);
}

int main(void)
{
    test_encode_health_frame_uses_expected_channel_layout();
    test_encode_health_frame_rejects_invalid_arguments();
    test_encode_jy901s_frame_keeps_ten_channel_order();
    test_encode_single_motor_frame_keeps_eight_channel_order();
    return 0;
}
