#ifndef BOARD_UART_H
#define BOARD_UART_H

#include <stdbool.h>
#include <stdint.h>

#include <FreeRTOS.h>
#include <queue.h>

void board_uart_enable_rx_interrupt(QueueHandle_t queue);
void board_uart_irq_handler(void);
bool board_uart_tx_full(void);
void board_uart_transmit(uint8_t byte);
uint32_t board_uart_rx_overflow_count(void);

#endif
