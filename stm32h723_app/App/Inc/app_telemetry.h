#ifndef APP_TELEMETRY_H
#define APP_TELEMETRY_H

#include "stm32h7xx_hal.h"

void h723_app_telemetry_init(void);
void h723_app_telemetry_step(void);
void h723_app_telemetry_on_uart_tx_complete(UART_HandleTypeDef *huart);
void h723_app_telemetry_on_uart_error(UART_HandleTypeDef *huart);

#endif
