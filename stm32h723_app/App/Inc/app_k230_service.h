#ifndef APP_K230_SERVICE_H
#define APP_K230_SERVICE_H

#include <stdint.h>

/** 初始化 K230 UART2 DMA 接收和协议解析状态。仅在任务上下文调用一次。 */
void h723_k230_service_init(void);

/** 解析 UART2 软件环形缓冲中的全部字节，并发布 Watch 测量快照。 */
void h723_k230_service_step(uint32_t now_ms);

/** UART2 ReceiveToIdle DMA 回调入口；仅复制接收字节并重新启动 DMA。 */
void h723_k230_on_uart2_rx_event(uint16_t size);

/** UART2 错误回调入口；记录错误后重新启动 DMA。 */
void h723_k230_on_uart2_error(void);

#endif
