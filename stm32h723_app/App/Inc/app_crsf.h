#ifndef APP_CRSF_H
#define APP_CRSF_H

#include <stdbool.h>
#include <stdint.h>

#define APP_CRSF_CHANNEL_COUNT 16U

typedef struct {
    uint16_t channels[APP_CRSF_CHANNEL_COUNT];
    uint32_t last_valid_ms;
    uint32_t valid_frame_count;
    uint32_t crc_error_count;
    uint32_t frame_error_count;
    bool valid;
} app_crsf_input_t;

typedef struct {
    uint8_t frame[64];
    uint8_t size;
    uint8_t expected_size;
} app_crsf_parser_t;

void app_crsf_parser_init(app_crsf_parser_t *parser);
bool app_crsf_parser_feed(app_crsf_parser_t *parser, uint8_t byte, uint32_t now_ms,
                          app_crsf_input_t *input);

#endif
