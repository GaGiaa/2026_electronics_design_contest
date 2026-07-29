#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "app_jy901s.h"

static void put_i16_le(uint8_t *data, int16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)((uint16_t)value >> 8U);
}

static void make_frame(uint8_t type, int16_t x, int16_t y, int16_t z,
                       int16_t extra, uint8_t frame[APP_JY901S_FRAME_SIZE])
{
    uint32_t index;
    uint8_t checksum = 0U;

    frame[0] = 0x55U;
    frame[1] = type;
    put_i16_le(&frame[2], x);
    put_i16_le(&frame[4], y);
    put_i16_le(&frame[6], z);
    put_i16_le(&frame[8], extra);
    for (index = 0U; index < APP_JY901S_FRAME_SIZE - 1U; ++index) {
        checksum = (uint8_t)(checksum + frame[index]);
    }
    frame[APP_JY901S_FRAME_SIZE - 1U] = checksum;
}

static void feed(app_jy901s_parser_t *parser, const uint8_t *data, uint32_t length,
                 uint32_t now_ms, app_jy901s_sample_t *sample, bool *published)
{
    uint32_t index;

    for (index = 0U; index < length; ++index) {
        if (app_jy901s_parser_feed(parser, data[index], now_ms, sample)) {
            *published = true;
        }
    }
}

static void test_publishes_accel_gyro_angle_and_temperature(void)
{
    app_jy901s_parser_t parser;
    app_jy901s_sample_t sample = {0};
    uint8_t accel[APP_JY901S_FRAME_SIZE];
    uint8_t gyro[APP_JY901S_FRAME_SIZE];
    uint8_t angle[APP_JY901S_FRAME_SIZE];
    bool published = false;

    make_frame(APP_JY901S_FRAME_ACCEL, 16384, -8192, 4096, 2534, accel);
    make_frame(APP_JY901S_FRAME_GYRO, 16384, -8192, 4096, 0, gyro);
    make_frame(APP_JY901S_FRAME_ANGLE, 16384, -8192, 4096, 0, angle);
    app_jy901s_parser_init(&parser);
    feed(&parser, accel, sizeof(accel), 100U, &sample, &published);
    feed(&parser, gyro, sizeof(gyro), 101U, &sample, &published);
    feed(&parser, angle, sizeof(angle), 102U, &sample, &published);

    assert(published);
    assert(sample.valid);
    assert(sample.last_sample_ms == 102U);
    assert(sample.acceleration_raw[0] == 16384);
    assert(sample.temperature_raw == 2534);
    assert(fabsf(sample.acceleration_g[0] - 8.0f) < 0.001f);
    assert(fabsf(sample.acceleration_g[1] + 4.0f) < 0.001f);
    assert(fabsf(sample.angular_rate_dps[0] - 1000.0f) < 0.001f);
    assert(fabsf(sample.angle_deg[0] - 90.0f) < 0.001f);
    assert(fabsf(sample.temperature_celsius - 25.34f) < 0.001f);
    assert(sample.acceleration_frame_count == 1U);
    assert(sample.gyro_frame_count == 1U);
    assert(sample.angle_frame_count == 1U);
    assert(sample.complete_sample_count == 1U);
}

static void test_rejects_bad_checksum_and_resynchronizes_after_noise(void)
{
    app_jy901s_parser_t parser;
    app_jy901s_sample_t sample = {0};
    uint8_t accel[APP_JY901S_FRAME_SIZE];
    uint8_t gyro[APP_JY901S_FRAME_SIZE];
    uint8_t angle[APP_JY901S_FRAME_SIZE];
    const uint8_t noise[] = {0x00U, 0x55U, 0x99U, 0x11U};
    bool published = false;

    make_frame(APP_JY901S_FRAME_ACCEL, 1, 2, 3, 2500, accel);
    accel[APP_JY901S_FRAME_SIZE - 1U] ^= 0x01U;
    make_frame(APP_JY901S_FRAME_GYRO, 4, 5, 6, 0, gyro);
    make_frame(APP_JY901S_FRAME_ANGLE, 7, 8, 9, 0, angle);
    app_jy901s_parser_init(&parser);
    feed(&parser, noise, sizeof(noise), 200U, &sample, &published);
    feed(&parser, accel, sizeof(accel), 201U, &sample, &published);
    feed(&parser, gyro, 4U, 202U, &sample, &published);
    feed(&parser, &gyro[4], sizeof(gyro) - 4U, 203U, &sample, &published);
    feed(&parser, angle, sizeof(angle), 204U, &sample, &published);

    assert(!published);
    assert(sample.checksum_error_count == 1U);
    assert(sample.gyro_frame_count == 1U);
    assert(sample.angle_frame_count == 1U);
    assert(sample.complete_sample_count == 0U);
}

int main(void)
{
    test_publishes_accel_gyro_angle_and_temperature();
    test_rejects_bad_checksum_and_resynchronizes_after_noise();
    return 0;
}
