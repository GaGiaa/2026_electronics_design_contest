#include "app_k230.h"

#include <math.h>
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
    sample->pixel_x = h723_k230_read_float_le(&parser->frame[3]);
    sample->ball_position_mm = app_k230_pixel_to_ball_mm(sample->pixel_x, 0.0f);
    sample->last_frame_ms = now_ms;
    ++sample->valid_frame_count;
    return true;
}

float app_k230_pixel_to_ball_mm(float pixel_x, float pipe_tilt_deg)
{
    const float delta = (pixel_x - APP_K230_PX_CENTER) / APP_K230_PX_PER_MM;
    const float sin_phi = sinf(pipe_tilt_deg * 0.01745329252f);
    const float correction = 1.0f + delta * sin_phi / APP_K230_CAMERA_HEIGHT_MM;
    float position_mm;

    if (correction > -0.001f && correction < 0.001f) {
        return 0.0f;
    }
    position_mm = delta / correction;
    if (position_mm > APP_K230_BALL_TRAVEL_MAX_MM) {
        return APP_K230_BALL_TRAVEL_MAX_MM;
    }
    if (position_mm < -APP_K230_BALL_TRAVEL_MAX_MM) {
        return -APP_K230_BALL_TRAVEL_MAX_MM;
    }
    return position_mm;
}
