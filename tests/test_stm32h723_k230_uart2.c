#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "app_k230.h"

static uint16_t crc16_ccitt_false(const uint8_t *data, uint32_t length)
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

static void put_float_le(uint8_t *data, float value)
{
    union {
        float value;
        uint32_t bits;
    } converter;

    converter.value = value;
    data[0] = (uint8_t)converter.bits;
    data[1] = (uint8_t)(converter.bits >> 8U);
    data[2] = (uint8_t)(converter.bits >> 16U);
    data[3] = (uint8_t)(converter.bits >> 24U);
}

static void make_frame(uint8_t valid, float pixel_x, uint8_t frame[APP_K230_FRAME_SIZE])
{
    uint16_t crc;

    frame[0] = APP_K230_SOF0;
    frame[1] = APP_K230_SOF1;
    frame[2] = valid;
    put_float_le(&frame[3], pixel_x);
    crc = crc16_ccitt_false(frame, APP_K230_FRAME_SIZE - 2U);
    frame[7] = (uint8_t)crc;
    frame[8] = (uint8_t)(crc >> 8U);
}

static bool feed(app_k230_parser_t *parser, const uint8_t *data, uint32_t length,
                 uint32_t now_ms, app_k230_sample_t *sample)
{
    bool published = false;
    uint32_t index;

    for (index = 0U; index < length; ++index) {
        if (app_k230_parser_feed(parser, data[index], now_ms, sample)) {
            published = true;
        }
    }
    return published;
}

static void test_publishes_little_endian_pixel_x(void)
{
    app_k230_parser_t parser;
    app_k230_sample_t sample = {0};
    uint8_t frame[APP_K230_FRAME_SIZE];

    make_frame(APP_K230_VALID_TRUE, -12.5f, frame);
    app_k230_parser_init(&parser);

    assert(feed(&parser, frame, sizeof(frame), 123U, &sample));
    assert(sample.valid);
    assert(fabsf(sample.pixel_x + 12.5f) < 0.0001f);
    assert(sample.ball_position_mm == -120.0f);
    assert(sample.last_frame_ms == 123U);
    assert(sample.valid_frame_count == 1U);
}

static void test_publishes_invalid_frame_with_raw_pixel_x(void)
{
    app_k230_parser_t parser;
    app_k230_sample_t sample = {0};
    uint8_t frame[APP_K230_FRAME_SIZE];

    make_frame(APP_K230_VALID_FALSE, 99.0f, frame);
    app_k230_parser_init(&parser);

    assert(feed(&parser, frame, sizeof(frame), 456U, &sample));
    assert(!sample.valid);
    assert(sample.pixel_x == 99.0f);
    assert(sample.ball_position_mm == -120.0f);
    assert(sample.last_frame_ms == 456U);
    assert(sample.valid_frame_count == 1U);
}

static void test_rejects_bad_crc_and_resynchronizes_after_noise(void)
{
    app_k230_parser_t parser;
    app_k230_sample_t sample = {0};
    uint8_t bad_frame[APP_K230_FRAME_SIZE];
    uint8_t valid_frame[APP_K230_FRAME_SIZE];
    const uint8_t noise[] = {0x00U, APP_K230_SOF0, 0x11U, 0x7FU};

    make_frame(APP_K230_VALID_TRUE, 10.0f, bad_frame);
    bad_frame[8] ^= 0x01U;
    make_frame(APP_K230_VALID_TRUE, 3.25f, valid_frame);
    app_k230_parser_init(&parser);

    assert(!feed(&parser, noise, sizeof(noise), 1U, &sample));
    assert(!feed(&parser, bad_frame, sizeof(bad_frame), 2U, &sample));
    assert(feed(&parser, valid_frame, sizeof(valid_frame), 3U, &sample));
    assert(sample.crc_error_count == 1U);
    assert(sample.valid);
    assert(fabsf(sample.pixel_x - 3.25f) < 0.0001f);
}

static void test_rejects_invalid_valid_field_without_overwriting_sample(void)
{
    app_k230_parser_t parser;
    app_k230_sample_t sample = {0};
    uint8_t valid_frame[APP_K230_FRAME_SIZE];
    uint8_t malformed_frame[APP_K230_FRAME_SIZE];

    make_frame(APP_K230_VALID_TRUE, 8.0f, valid_frame);
    make_frame(2U, 42.0f, malformed_frame);
    app_k230_parser_init(&parser);

    assert(feed(&parser, valid_frame, sizeof(valid_frame), 5U, &sample));
    assert(!feed(&parser, malformed_frame, sizeof(malformed_frame), 6U, &sample));
    assert(sample.format_error_count == 1U);
    assert(sample.valid);
    assert(fabsf(sample.pixel_x - 8.0f) < 0.0001f);
    assert(sample.valid_frame_count == 1U);
}

static void test_pixel_to_ball_mm_applies_perspective_and_limits(void)
{
    const float center = app_k230_pixel_to_ball_mm(APP_K230_PX_CENTER, 6.35f);
    const float positive = app_k230_pixel_to_ball_mm(
        APP_K230_PX_CENTER + 6.575f * 10.0f, 6.35f);
    const float negative = app_k230_pixel_to_ball_mm(
        APP_K230_PX_CENTER - 6.575f * 10.0f, 6.35f);

    assert(fabsf(center) < 0.0001f);
    assert(positive > 9.0f && positive < 10.0f);
    assert(negative < -10.0f && negative > -11.0f);
    assert(app_k230_pixel_to_ball_mm(0.0f, 0.0f) == -120.0f);
    assert(app_k230_pixel_to_ball_mm(3000.0f, 0.0f) == 120.0f);
}

int main(void)
{
    test_publishes_little_endian_pixel_x();
    test_publishes_invalid_frame_with_raw_pixel_x();
    test_rejects_bad_crc_and_resynchronizes_after_noise();
    test_rejects_invalid_valid_field_without_overwriting_sample();
    test_pixel_to_ball_mm_applies_perspective_and_limits();
    return 0;
}
