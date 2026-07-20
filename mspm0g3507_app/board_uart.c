#include "board_uart.h"

#include <FreeRTOS.h>
#include <task.h>

#include "ti_msp_dl_config.h"

static QueueHandle_t g_rx_queue;
static volatile uint32_t g_rx_overflow_count;

void board_uart_enable_rx_interrupt(QueueHandle_t queue)
{
    g_rx_queue = queue;
    NVIC_SetPriority(UART_0_INST_INT_IRQN, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
}

void board_uart_irq_handler(void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (DL_UART_Main_getPendingInterrupt(UART_0_INST) == DL_UART_MAIN_IIDX_RX) {
        while (!DL_UART_Main_isRXFIFOEmpty(UART_0_INST)) {
        uint8_t byte = DL_UART_Main_receiveData(UART_0_INST);
        if ((g_rx_queue == NULL) ||
            (xQueueSendFromISR(g_rx_queue, &byte, &higher_priority_task_woken) != pdPASS)) {
            ++g_rx_overflow_count;
        }
        }
    }
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

bool board_uart_tx_full(void) { return DL_UART_Main_isTXFIFOFull(UART_0_INST); }
void board_uart_transmit(uint8_t byte) { DL_UART_Main_transmitData(UART_0_INST, byte); }
uint32_t board_uart_rx_overflow_count(void) { return g_rx_overflow_count; }
