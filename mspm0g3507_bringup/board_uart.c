#include "board_uart.h"

#include "ti_msp_dl_config.h"

static EchoQueue *g_rx_queue;

void board_uart_enable_rx_interrupt(EchoQueue *queue)
{
    g_rx_queue = queue;
    NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
}

void board_uart_irq_handler(void)
{
    if (DL_UART_Main_getPendingInterrupt(UART_0_INST) == DL_UART_MAIN_IIDX_RX) {
        uint8_t byte = DL_UART_Main_receiveData(UART_0_INST);

        if (g_rx_queue != NULL) {
            (void) echo_queue_push(g_rx_queue, byte);
        }
    }
}

bool board_uart_tx_full(void)
{
    return DL_UART_Main_isTXFIFOFull(UART_0_INST);
}

void board_uart_transmit(uint8_t byte)
{
    DL_UART_Main_transmitData(UART_0_INST, byte);
}
