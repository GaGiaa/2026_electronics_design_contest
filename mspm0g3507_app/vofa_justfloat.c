#include "vofa_justfloat.h"

#include <string.h>

static void write_float32_le(uint8_t *destination, float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    destination[0] = (uint8_t)(bits & 0xFFU);
    destination[1] = (uint8_t)((bits >> 8U) & 0xFFU);
    destination[2] = (uint8_t)((bits >> 16U) & 0xFFU);
    destination[3] = (uint8_t)((bits >> 24U) & 0xFFU);
}

bool vofa_justfloat_encode(uint8_t *frame, size_t frame_size,
                           const float *channels, size_t channel_count)
{
    size_t channel;
    size_t payload_size;

    if ((frame == NULL) || (channels == NULL) || (channel_count == 0U) ||
        (frame_size != VOFA_JUSTFLOAT_FRAME_SIZE(channel_count)) ||
        (sizeof(float) != VOFA_JUSTFLOAT_CHANNEL_SIZE)) {
        return false;
    }

    for (channel = 0U; channel < channel_count; ++channel) {
        write_float32_le(&frame[channel * VOFA_JUSTFLOAT_CHANNEL_SIZE],
                         channels[channel]);
    }
    payload_size = channel_count * VOFA_JUSTFLOAT_CHANNEL_SIZE;
    frame[payload_size] = 0x00U;
    frame[payload_size + 1U] = 0x00U;
    frame[payload_size + 2U] = 0x80U;
    frame[payload_size + 3U] = 0x7FU;
    return true;
}

bool vofa_justfloat_encode3(uint8_t *frame, size_t frame_size,
                             float channel0, float channel1, float channel2)
{
    const float channels[VOFA_JUSTFLOAT_CHANNEL_COUNT] = {
        channel0, channel1, channel2
    };

    return vofa_justfloat_encode(frame, frame_size, channels,
                                 VOFA_JUSTFLOAT_CHANNEL_COUNT);
}
