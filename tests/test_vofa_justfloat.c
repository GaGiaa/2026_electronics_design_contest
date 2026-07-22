#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "vofa_justfloat.h"

#define GRAY_TEST_CHANNEL_COUNT 22U

static void test_encode22_writes_little_endian_frame(void)
{
    const float channels[GRAY_TEST_CHANNEL_COUNT] = {
        394.0f, 1311.0f, 156.0f, 153.0f, 1418.0f, 1021.0f, 2683.0f, 215.0f,
        67.0f, 485.0f, 34.0f, 0.0f, 772.0f, 718.0f, 2316.0f, 182.0f,
        0.0f, 0xFFU, 8.0f, -226.0f, 28186.0f, 42.0f
    };
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(GRAY_TEST_CHANNEL_COUNT)] = {0U};
    size_t last_offset = (GRAY_TEST_CHANNEL_COUNT - 1U) * VOFA_JUSTFLOAT_CHANNEL_SIZE;

    assert(vofa_justfloat_encode(frame, sizeof(frame), channels,
                                  GRAY_TEST_CHANNEL_COUNT));
    assert(frame[0U] == 0x00U);
    assert(frame[1U] == 0x00U);
    assert(frame[2U] == 0xC5U);
    assert(frame[3U] == 0x43U);
    assert(frame[last_offset] == 0x00U);
    assert(frame[last_offset + 1U] == 0x00U);
    assert(frame[last_offset + 2U] == 0x28U);
    assert(frame[last_offset + 3U] == 0x42U);
    assert(frame[sizeof(frame) - 4U] == 0x00U);
    assert(frame[sizeof(frame) - 3U] == 0x00U);
    assert(frame[sizeof(frame) - 2U] == 0x80U);
    assert(frame[sizeof(frame) - 1U] == 0x7FU);
}

static void test_encode_rejects_invalid_buffers(void)
{
    const float channels[GRAY_TEST_CHANNEL_COUNT] = {0};
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(GRAY_TEST_CHANNEL_COUNT)] = {0U};

    assert(!vofa_justfloat_encode(NULL, sizeof(frame), channels,
                                   GRAY_TEST_CHANNEL_COUNT));
    assert(!vofa_justfloat_encode(frame, sizeof(frame) - 1U, channels,
                                   GRAY_TEST_CHANNEL_COUNT));
    assert(!vofa_justfloat_encode(frame, sizeof(frame), NULL,
                                   GRAY_TEST_CHANNEL_COUNT));
    assert(!vofa_justfloat_encode(frame, sizeof(frame), channels, 0U));
}

static void test_encode3_compatibility_remains(void)
{
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE_3] = {0U};

    assert(vofa_justfloat_encode3(frame, sizeof(frame), 1.0f, 2.0f, 3.0f));
    assert(frame[0U] == 0x00U);
    assert(frame[1U] == 0x00U);
    assert(frame[2U] == 0x80U);
    assert(frame[3U] == 0x3FU);
}

int main(void)
{
    test_encode22_writes_little_endian_frame();
    test_encode_rejects_invalid_buffers();
    test_encode3_compatibility_remains();
    return 0;
}
