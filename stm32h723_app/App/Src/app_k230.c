#include "app_k230.h"

#include <string.h>

static uint16_t h723_k230_crc16_ccitt_false(const uint8_t *data, uint32_t length)
{
    uint16_t crc = 0xFFFFU;
    uint32_t index;
    uint32_t bit;

    for (index = 0U; index < length; ++index) {
        crc ^= (uint16_t)data[index] << 8U;
        for (bit = 0U; bit < 8U; ++bit) {
            crc = (crc & 0x8000U) != 0U ? (uint16_t)((crc << 1U) ^ 0x1021U) :
                                          (uint16_t)(crc << 1U);
        }
    }
    return crc;
}

static uint16_t h723_k230_read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8U);
}

static float h723_k230_read_float_le(const uint8_t *data)
{
    union {
        float value;
        uint32_t bits;
    } converter;

    converter.bits = (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
                     ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
    return converter.value;
}

void app_k230_parser_init(app_k230_parser_t *parser)
{
    memset(parser, 0, sizeof(*parser));
}

bool app_k230_parser_feed(app_k230_parser_t *parser, uint8_t byte,
                          uint32_t now_ms, app_k230_sample_t *sample)
{
    uint16_t received_crc;

    if (parser->index == 0U) {
        if (byte == APP_K230_SOF0) {
            parser->frame[parser->index++] = byte;
        }
        return false;
    }
    if (parser->index == 1U && byte != APP_K230_SOF1) {
        ++sample->format_error_count;
        parser->index = byte == APP_K230_SOF0 ? 1U : 0U;
        if (parser->index == 1U) {
            parser->frame[0] = byte;
        }
        return false;
    }

    parser->frame[parser->index++] = byte;
    if (parser->index < APP_K230_FRAME_SIZE) {
        return false;
    }
    parser->index = 0U;
    received_crc = h723_k230_read_u16_le(&parser->frame[APP_K230_FRAME_SIZE - 2U]);
    if (h723_k230_crc16_ccitt_false(parser->frame, APP_K230_FRAME_SIZE - 2U) != received_crc) {
        ++sample->crc_error_count;
        return false;
    }
    if (parser->frame[2] != APP_K230_VALID_FALSE && parser->frame[2] != APP_K230_VALID_TRUE) {
        ++sample->format_error_count;
        return false;
    }

    sample->valid = parser->frame[2] == APP_K230_VALID_TRUE;
    sample->distance_mm = sample->valid ? h723_k230_read_float_le(&parser->frame[3]) : 0.0f;
    sample->last_frame_ms = now_ms;
    ++sample->valid_frame_count;
    return true;
}
