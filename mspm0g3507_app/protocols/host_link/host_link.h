#ifndef HOST_LINK_H
#define HOST_LINK_H

#include <stddef.h>
#include <stdint.h>

#define HOST_LINK_MAX_LINE_LENGTH 64U

typedef struct host_link host_link_t;

typedef void (*host_link_response_callback_t)(const uint8_t *data,
                                               size_t length,
                                               void *context);

struct host_link {
    char line[HOST_LINK_MAX_LINE_LENGTH + 1U];
    size_t length;
    uint8_t discarding_line;
    host_link_response_callback_t response_callback;
    void *response_context;
};

/**
 * @brief 初始化主机通信行协议解析器。
 *
 * 初始化后的解析器只能在一个调用上下文中串行使用。响应回调在
 * host_link_receive_byte() 内同步调用，回调不得阻塞或保留 data 指针。
 */
void host_link_init(host_link_t *link,
                    host_link_response_callback_t response_callback,
                    void *response_context);

/**
 * @brief 输入一个来自 UART2 的字节并处理完整命令行。
 *
 * 支持以 LF 或 CRLF 结束的 ASCII 命令。空行被忽略；超过
 * HOST_LINK_MAX_LINE_LENGTH 的行会被丢弃，并返回 ERR,LONG。
 */
void host_link_receive_byte(host_link_t *link, uint8_t byte);

#endif
