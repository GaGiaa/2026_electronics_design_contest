#ifndef APP_K230_H
#define APP_K230_H

#include <stdbool.h>
#include <stdint.h>

#define APP_K230_FRAME_SIZE 9U
#define APP_K230_SOF0 0xA5U
#define APP_K230_SOF1 0x5AU
#define APP_K230_VALID_FALSE 0x00U
#define APP_K230_VALID_TRUE 0x01U

/* K230 pixel-to-pipe calibration for the 1920x1080 camera view. */
#define APP_K230_PX_CENTER 993.0f
#define APP_K230_PX_PER_MM 6.575f
#define APP_K230_CAMERA_HEIGHT_MM 24.5f
#define APP_K230_BALL_TRAVEL_MAX_MM 120.0f

/** K230 视觉协议的最新合法帧快照。pixel_x 是原始像素坐标。 */
typedef struct {
    float pixel_x;
    float ball_position_mm;
    uint32_t valid_frame_count;
    uint32_t crc_error_count;
    uint32_t format_error_count;
    uint32_t last_frame_ms;
    bool valid;
} app_k230_sample_t;

/** K230 固定 9 字节帧的逐字节解析状态。初始化后仅由一个任务上下文调用。 */
typedef struct {
    uint8_t frame[APP_K230_FRAME_SIZE];
    uint8_t index;
} app_k230_parser_t;

/**
 * @brief 初始化 K230 帧解析器。
 *
 * @param[out] parser 要初始化的解析状态，不能为空。
 */
void app_k230_parser_init(app_k230_parser_t *parser);

/**
 * @brief 向解析器输入一个 UART 字节，并在收到合法完整帧时更新测量快照。
 *
 * CRC 覆盖包头、有效标志和像素字段。`valid=0` 的合法帧仍保留原始像素值；
 * CRC 或格式错误帧不覆盖既有快照。
 *
 * @param[in,out] parser 已初始化的解析状态。
 * @param[in] byte 当前接收字节。
 * @param[in] now_ms 本地接收时间，单位 ms。
 * @param[in,out] sample 最新测量快照和错误计数。
 *
 * @return 收到并发布合法完整帧时返回 true，否则返回 false。
 */
bool app_k230_parser_feed(app_k230_parser_t *parser, uint8_t byte,
                          uint32_t now_ms, app_k230_sample_t *sample);

/** Convert a camera X coordinate to the ball position on the tilted pipe. */
float app_k230_pixel_to_ball_mm(float pixel_x, float pipe_tilt_deg);

#endif
