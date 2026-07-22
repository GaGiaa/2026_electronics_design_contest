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

bool vofa_justfloat_encode3(uint8_t *frame, size_t frame_size,
                             float channel0, float channel1, float channel2)
{
    if ((frame == NULL) || (frame_size != VOFA_JUSTFLOAT_FRAME_SIZE) ||
        (sizeof(float) != VOFA_JUSTFLOAT_CHANNEL_SIZE)) {
        return false;
    }

    write_float32_le(&frame[0U], channel0);
    write_float32_le(&frame[4U], channel1);
    write_float32_le(&frame[8U], channel2);
    frame[12U] = 0x00U;
    frame[13U] = 0x00U;
    frame[14U] = 0x80U;
    frame[15U] = 0x7FU;
    return true;
}
