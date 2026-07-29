#ifndef APP_JY901S_H
#define APP_JY901S_H

#include <stdbool.h>
#include <stdint.h>

#define APP_JY901S_FRAME_SIZE 11U
#define APP_JY901S_FRAME_ACCEL 0x51U
#define APP_JY901S_FRAME_GYRO 0x52U
#define APP_JY901S_FRAME_ANGLE 0x53U

typedef struct {
    int16_t acceleration_raw[3];
    int16_t temperature_raw;
    int16_t angular_rate_raw[3];
    int16_t angle_raw[3];
    float acceleration_g[3];
    float temperature_celsius;
    float angular_rate_dps[3];
    float angle_deg[3];
    uint32_t acceleration_frame_count;
    uint32_t gyro_frame_count;
    uint32_t angle_frame_count;
    uint32_t checksum_error_count;
    uint32_t format_error_count;
    uint32_t complete_sample_count;
    uint32_t last_sample_ms;
    bool valid;
} app_jy901s_sample_t;

typedef struct {
    uint8_t frame[APP_JY901S_FRAME_SIZE];
    uint8_t index;
    uint8_t received_mask;
} app_jy901s_parser_t;

void app_jy901s_parser_init(app_jy901s_parser_t *parser);
bool app_jy901s_parser_feed(app_jy901s_parser_t *parser, uint8_t byte,
                            uint32_t now_ms, app_jy901s_sample_t *sample);

#endif
