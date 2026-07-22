#ifndef CRSF_PROTOCOL_H
#define CRSF_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* CRSF 一帧最多包含的遥控通道数量。 */
#define CRSF_CHANNEL_COUNT 16U
/* 16 个通道按每通道 11 bit 打包后的载荷长度，单位为字节。 */
#define CRSF_CHANNEL_PAYLOAD_SIZE 22U
/* 解析器内部接收缓冲区的最大帧长度，包含地址和长度字段。 */
#define CRSF_MAX_FRAME_SIZE 64U
/* CRSF 长度字段的最大允许值，不包含地址和长度字段本身。 */
#define CRSF_MAX_FRAME_LENGTH (CRSF_MAX_FRAME_SIZE - 2U)
/* CRSF 接收头发送到飞控的设备地址。 */
#define CRSF_ADDRESS_RECEIVER 0xC8U
#define CRSF_ADDRESS_BROADCAST 0x00U
/* 打包遥控通道帧类型：RC_CHANNELS_PACKED。 */
#define CRSF_FRAME_TYPE_RC_CHANNELS_PACKED 0x16U
/* RC_CHANNELS_PACKED 的长度字段值：类型 + 22 字节载荷 + CRC。 */
#define CRSF_RC_CHANNELS_FRAME_LENGTH (1U + CRSF_CHANNEL_PAYLOAD_SIZE + 1U)

typedef struct {
    /* 解包后的 16 个 11-bit 原始通道值。 */
    uint16_t channels[CRSF_CHANNEL_COUNT];
} crsf_channels_t;

typedef struct {
    /* 当前正在接收的 CRSF 原始帧缓冲区。 */
    uint8_t frame[CRSF_MAX_FRAME_SIZE];
    /* 当前已接收的字节数量。 */
    uint8_t size;
    /* 根据长度字段计算出的完整帧字节数量。 */
    uint8_t expected_size;
    /* CRC 校验失败的帧数量。 */
    uint32_t crc_error_count;
    /* 长度非法等帧格式错误的数量。 */
    uint32_t frame_error_count;
} crsf_parser_t;

void crsf_parser_init(crsf_parser_t *parser);
bool crsf_parser_feed(crsf_parser_t *parser, uint8_t byte,
                      crsf_channels_t *channels);

#endif
