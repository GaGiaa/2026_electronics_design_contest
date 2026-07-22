#ifndef VOFA_JUSTFLOAT_H
#define VOFA_JUSTFLOAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VOFA_JUSTFLOAT_CHANNEL_COUNT 3U
#define VOFA_JUSTFLOAT_CHANNEL_SIZE 4U
#define VOFA_JUSTFLOAT_TAIL_SIZE 4U
#define VOFA_JUSTFLOAT_FRAME_SIZE \
    ((VOFA_JUSTFLOAT_CHANNEL_COUNT * VOFA_JUSTFLOAT_CHANNEL_SIZE) + VOFA_JUSTFLOAT_TAIL_SIZE)

bool vofa_justfloat_encode3(uint8_t *frame, size_t frame_size,
                             float channel0, float channel1, float channel2);

#endif
