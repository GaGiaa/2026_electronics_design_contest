#include "app_crsf.h"

#include <stddef.h>
#include <string.h>

static uint8_t app_crsf_crc8(const uint8_t *data, uint32_t length)
{
    uint8_t crc = 0U;
    uint32_t index;
    uint8_t bit;
    for (index = 0U; index < length; ++index) {
        crc ^= data[index];
        for (bit = 0U; bit < 8U; ++bit) {
            crc = (crc & 0x80U) != 0U ? (uint8_t)((crc << 1U) ^ 0xD5U) : (uint8_t)(crc << 1U);
        }
    }
    return crc;
}

static void app_crsf_reset(app_crsf_parser_t *parser)
{
    parser->size = 0U;
    parser->expected_size = 0U;
}

void app_crsf_parser_init(app_crsf_parser_t *parser)
{
    if (parser != NULL) { memset(parser, 0, sizeof(*parser)); }
}

bool app_crsf_parser_feed(app_crsf_parser_t *parser, uint8_t byte, uint32_t now_ms,
                          app_crsf_input_t *input)
{
    uint32_t channel;
    uint8_t length;
    if (parser == NULL || input == NULL) { return false; }
    if (parser->size == 0U) {
        if (byte == 0xC8U || byte == 0x00U) { parser->frame[parser->size++] = byte; }
        return false;
    }
    if (parser->size == 1U) {
        length = byte;
        if (length < 2U || length > 62U) { ++input->frame_error_count; app_crsf_reset(parser); return false; }
        parser->frame[parser->size++] = byte;
        parser->expected_size = (uint8_t)(length + 2U);
        return false;
    }
    parser->frame[parser->size++] = byte;
    if (parser->size < parser->expected_size) { return false; }
    if (app_crsf_crc8(&parser->frame[2], (uint32_t)parser->frame[1] - 1U) != parser->frame[parser->size - 1U]) {
        ++input->crc_error_count; app_crsf_reset(parser); return false;
    }
    if (parser->frame[1] == 24U && parser->frame[2] == 0x16U) {
        for (channel = 0U; channel < APP_CRSF_CHANNEL_COUNT; ++channel) {
            uint32_t offset = channel * 11U;
            uint32_t index = offset / 8U;
            uint32_t packed = (uint32_t)parser->frame[3U + index] |
                              ((uint32_t)parser->frame[4U + index] << 8U) |
                              ((uint32_t)parser->frame[5U + index] << 16U);
            input->channels[channel] = (uint16_t)((packed >> (offset % 8U)) & 0x07FFU);
        }
        input->last_valid_ms = now_ms;
        input->valid = true;
        ++input->valid_frame_count;
        app_crsf_reset(parser);
        return true;
    }
    app_crsf_reset(parser);
    return false;
}
