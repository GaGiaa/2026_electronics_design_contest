#include "crsf_protocol.h"

#include <string.h>

static uint8_t crsf_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0U;
    size_t index;

    for (index = 0U; index < length; ++index) {
        uint8_t bit;

        crc ^= data[index];
        for (bit = 0U; bit < 8U; ++bit) {
            crc = (crc & 0x80U) != 0U ? (uint8_t)((crc << 1U) ^ 0xD5U)
                                      : (uint8_t)(crc << 1U);
        }
    }
    return crc;
}

static void crsf_parser_reset(crsf_parser_t *parser)
{
    parser->size = 0U;
    parser->expected_size = 0U;
}

static bool crsf_is_valid_address(uint8_t byte)
{
    return byte == CRSF_ADDRESS_RECEIVER || byte == CRSF_ADDRESS_BROADCAST;
}

static bool crsf_decode_channels(const uint8_t payload[CRSF_CHANNEL_PAYLOAD_SIZE],
                                 crsf_channels_t *channels)
{
    size_t channel;

    if (channels == NULL) {
        return false;
    }

    for (channel = 0U; channel < CRSF_CHANNEL_COUNT; ++channel) {
        size_t bit_offset = channel * 11U;
        size_t byte_offset = bit_offset / 8U;
        uint32_t packed = (uint32_t)payload[byte_offset] |
                          ((uint32_t)payload[byte_offset + 1U] << 8U) |
                          ((uint32_t)payload[byte_offset + 2U] << 16U);
        channels->channels[channel] = (uint16_t)((packed >> (bit_offset % 8U)) & 0x07FFU);
    }
    return true;
}

void crsf_parser_init(crsf_parser_t *parser)
{
    if (parser == NULL) {
        return;
    }
    memset(parser, 0, sizeof(*parser));
}

bool crsf_parser_feed(crsf_parser_t *parser, uint8_t byte,
                      crsf_channels_t *channels)
{
    uint8_t length;

    if (parser == NULL) {
        return false;
    }

    if (parser->size == 0U) {
        if (crsf_is_valid_address(byte)) {
            parser->frame[0U] = byte;
            parser->size = 1U;
        }
        return false;
    }

    if (parser->size == 1U) {
        length = byte;
        if (length < 2U || length > CRSF_MAX_FRAME_LENGTH) {
            ++parser->frame_error_count;
            crsf_parser_reset(parser);
            if (crsf_is_valid_address(byte)) {
                parser->frame[0U] = byte;
                parser->size = 1U;
            }
            return false;
        }
        parser->frame[1U] = byte;
        parser->expected_size = (uint8_t)(length + 2U);
        parser->size = 2U;
        return false;
    }

    parser->frame[parser->size] = byte;
    ++parser->size;
    if (parser->size < parser->expected_size) {
        return false;
    }

    if (parser->size == parser->expected_size &&
        crsf_crc8(&parser->frame[2U], (size_t)parser->frame[1U] - 1U) !=
            parser->frame[parser->size - 1U]) {
        ++parser->crc_error_count;
        crsf_parser_reset(parser);
        return false;
    }

    if (parser->size == parser->expected_size &&
        parser->frame[2U] == CRSF_FRAME_TYPE_RC_CHANNELS_PACKED &&
        parser->frame[1U] == CRSF_RC_CHANNELS_FRAME_LENGTH) {
        bool decoded = crsf_decode_channels(&parser->frame[3U], channels);
        crsf_parser_reset(parser);
        return decoded;
    }

    crsf_parser_reset(parser);
    return false;
}
