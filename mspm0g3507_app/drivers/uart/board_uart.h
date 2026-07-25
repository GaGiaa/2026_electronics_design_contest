#ifndef BOARD_UART_H
#define BOARD_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <FreeRTOS.h>
#include <queue.h>

#define BOARD_UART_TX_MESSAGE_MAX_LENGTH 192U

void board_uart_enable_rx_interrupt(QueueHandle_t queue);
void board_uart_irq_handler(void);
void board_uart_tx_task(void *argument);
bool board_uart_tx_full(void);
void board_uart_transmit(uint8_t byte);
void board_uart_write(const uint8_t *data, size_t length);
uint32_t board_uart_rx_overflow_count(void);

#endif
