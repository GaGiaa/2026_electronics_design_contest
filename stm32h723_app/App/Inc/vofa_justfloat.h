#ifndef VOFA_JUSTFLOAT_H
#define VOFA_JUSTFLOAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VOFA_JUSTFLOAT_CHANNEL_SIZE 4U
#define VOFA_JUSTFLOAT_FRAME_TAIL_SIZE 4U
#define VOFA_JUSTFLOAT_FRAME_SIZE(channel_count) \
    (((channel_count) * VOFA_JUSTFLOAT_CHANNEL_SIZE) + VOFA_JUSTFLOAT_FRAME_TAIL_SIZE)

bool vofa_justfloat_encode(uint8_t *frame, size_t frame_size,
                           const float *channels, size_t channel_count);

#endif
