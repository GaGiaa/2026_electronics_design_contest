#ifndef STM32H723_SERVICE_TEST_USART_H
#define STM32H723_SERVICE_TEST_USART_H

#include <stdint.h>

typedef struct {
    void *hdmarx;
} UART_HandleTypeDef;

extern UART_HandleTypeDef huart7;
extern UART_HandleTypeDef huart8;
extern UART_HandleTypeDef huart9;

typedef enum {
    HAL_UART_TEST_OK = 0,
} HAL_UART_TestStatusTypeDef;

#define __HAL_DMA_DISABLE_IT(handle, interrupt) ((void)(handle), (void)(interrupt))

int HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *huart,
                                 uint8_t *buffer,
                                 uint16_t size);

#endif
