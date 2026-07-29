#include "vofa_justfloat.h"

#include <string.h>

bool vofa_justfloat_encode(uint8_t *frame, size_t frame_size,
                           const float *channels, size_t channel_count)
{
    size_t index;
    size_t required_size;

    if ((frame == NULL) || (channels == NULL) || (channel_count == 0U)) {
        return false;
    }

    required_size = VOFA_JUSTFLOAT_FRAME_SIZE(channel_count);
    if (frame_size < required_size) {
        return false;
    }

    for (index = 0U; index < channel_count; ++index) {
        memcpy(&frame[index * VOFA_JUSTFLOAT_CHANNEL_SIZE], &channels[index],
               VOFA_JUSTFLOAT_CHANNEL_SIZE);
    }

    frame[required_size - 4U] = 0x00U;
    frame[required_size - 3U] = 0x00U;
    frame[required_size - 2U] = 0x80U;
    frame[required_size - 1U] = 0x7FU;
    return true;
}
