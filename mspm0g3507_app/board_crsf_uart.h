#ifndef BOARD_CRSF_UART_H
#define BOARD_CRSF_UART_H

#include <stdbool.h>
#include <stdint.h>

#ifndef BOARD_CRSF_UART_RX_BUFFER_LENGTH
#define BOARD_CRSF_UART_RX_BUFFER_LENGTH 256U
#endif

void board_crsf_uart_enable_rx_interrupt(void);
void board_crsf_uart_irq_handler(void);
bool board_crsf_uart_read_byte(uint8_t *byte);
uint32_t board_crsf_uart_rx_overflow_count(void);

#endif
