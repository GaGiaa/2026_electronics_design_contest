#ifndef BOARD_UART_H
#define BOARD_UART_H

#include "echo_queue.h"

void board_uart_enable_rx_interrupt(EchoQueue *queue);
void board_uart_irq_handler(void);
bool board_uart_tx_full(void);
void board_uart_transmit(uint8_t byte);

#endif
