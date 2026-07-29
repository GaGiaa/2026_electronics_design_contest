#include "app_jy901s.h"

#include <string.h>

#define H723_JY901S_ACCEL_SCALE_G (16.0f / 32768.0f)
#define H723_JY901S_GYRO_SCALE_DPS (2000.0f / 32768.0f)
#define H723_JY901S_ANGLE_SCALE_DEG (180.0f / 32768.0f)
#define H723_JY901S_TEMPERATURE_SCALE_C 0.01f
#define H723_JY901S_ACCEL_MASK 0x01U
#define H723_JY901S_GYRO_MASK 0x02U
#define H723_JY901S_ANGLE_MASK 0x04U
#define H723_JY901S_COMPLETE_MASK (H723_JY901S_ACCEL_MASK | H723_JY901S_GYRO_MASK | H723_JY901S_ANGLE_MASK)

static int16_t h723_jy901s_read_i16_le(const uint8_t *data)
{
    return (int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
}

static uint8_t h723_jy901s_checksum(const uint8_t frame[APP_JY901S_FRAME_SIZE])
{
    uint8_t checksum = 0U;
    uint32_t index;

    for (index = 0U; index < APP_JY901S_FRAME_SIZE - 1U; ++index) {
        checksum = (uint8_t)(checksum + frame[index]);
    }
    return checksum;
}

void app_jy901s_parser_init(app_jy901s_parser_t *parser)
{
    memset(parser, 0, sizeof(*parser));
}

bool app_jy901s_parser_feed(app_jy901s_parser_t *parser, uint8_t byte,
                            uint32_t now_ms, app_jy901s_sample_t *sample)
{
    uint8_t type;
    int16_t x;
    int16_t y;
    int16_t z;

    if (parser->index == 0U) {
        if (byte == 0x55U) { parser->frame[parser->index++] = byte; }
        return false;
    }
    if (parser->index == 1U && (byte < APP_JY901S_FRAME_ACCEL || byte > APP_JY901S_FRAME_ANGLE)) {
        ++sample->format_error_count;
        parser->index = byte == 0x55U ? 1U : 0U;
        if (parser->index == 1U) { parser->frame[0] = byte; }
        return false;
    }

    parser->frame[parser->index++] = byte;
    if (parser->index < APP_JY901S_FRAME_SIZE) { return false; }
    parser->index = 0U;
    if (h723_jy901s_checksum(parser->frame) != parser->frame[APP_JY901S_FRAME_SIZE - 1U]) {
        ++sample->checksum_error_count;
        return false;
    }

    type = parser->frame[1];
    x = h723_jy901s_read_i16_le(&parser->frame[2]);
    y = h723_jy901s_read_i16_le(&parser->frame[4]);
    z = h723_jy901s_read_i16_le(&parser->frame[6]);
    if (type == APP_JY901S_FRAME_ACCEL) {
        sample->acceleration_raw[0] = x;
        sample->acceleration_raw[1] = y;
        sample->acceleration_raw[2] = z;
        sample->temperature_raw = h723_jy901s_read_i16_le(&parser->frame[8]);
        sample->acceleration_g[0] = (float)x * H723_JY901S_ACCEL_SCALE_G;
        sample->acceleration_g[1] = (float)y * H723_JY901S_ACCEL_SCALE_G;
        sample->acceleration_g[2] = (float)z * H723_JY901S_ACCEL_SCALE_G;
        sample->temperature_celsius = (float)sample->temperature_raw * H723_JY901S_TEMPERATURE_SCALE_C;
        ++sample->acceleration_frame_count;
        parser->received_mask |= H723_JY901S_ACCEL_MASK;
    } else if (type == APP_JY901S_FRAME_GYRO) {
        sample->angular_rate_raw[0] = x;
        sample->angular_rate_raw[1] = y;
        sample->angular_rate_raw[2] = z;
        sample->angular_rate_dps[0] = (float)x * H723_JY901S_GYRO_SCALE_DPS;
        sample->angular_rate_dps[1] = (float)y * H723_JY901S_GYRO_SCALE_DPS;
        sample->angular_rate_dps[2] = (float)z * H723_JY901S_GYRO_SCALE_DPS;
        ++sample->gyro_frame_count;
        parser->received_mask |= H723_JY901S_GYRO_MASK;
    } else {
        sample->angle_raw[0] = x;
        sample->angle_raw[1] = y;
        sample->angle_raw[2] = z;
        sample->angle_deg[0] = (float)x * H723_JY901S_ANGLE_SCALE_DEG;
        sample->angle_deg[1] = (float)y * H723_JY901S_ANGLE_SCALE_DEG;
        sample->angle_deg[2] = (float)z * H723_JY901S_ANGLE_SCALE_DEG;
        ++sample->angle_frame_count;
        parser->received_mask |= H723_JY901S_ANGLE_MASK;
    }
    if (parser->received_mask != H723_JY901S_COMPLETE_MASK) { return false; }

    parser->received_mask = 0U;
    sample->last_sample_ms = now_ms;
    sample->valid = true;
    ++sample->complete_sample_count;
    return true;
}
