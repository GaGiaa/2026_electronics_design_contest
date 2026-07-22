#include "board_crsf_uart.h"

#include <FreeRTOS.h>

#include "ti_msp_dl_config.h"

static volatile uint8_t g_crsf_rx_buffer[BOARD_CRSF_UART_RX_BUFFER_LENGTH];
static volatile uint16_t g_crsf_rx_head;
static volatile uint16_t g_crsf_rx_tail;
static volatile uint32_t g_crsf_rx_overflow_count;

static uint16_t board_crsf_uart_next_index(uint16_t index)
{
    ++index;
    if (index >= BOARD_CRSF_UART_RX_BUFFER_LENGTH) {
        index = 0U;
    }
    return index;
}

void board_crsf_uart_enable_rx_interrupt(void)
{
    NVIC_SetPriority(UART_3_INST_INT_IRQN, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_ClearPendingIRQ(UART_3_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_3_INST_INT_IRQN);
}

void board_crsf_uart_irq_handler(void)
{
    if (DL_UART_Main_getPendingInterrupt(UART_3_INST) == DL_UART_MAIN_IIDX_RX) {
        while (!DL_UART_Main_isRXFIFOEmpty(UART_3_INST)) {
            uint8_t byte = DL_UART_Main_receiveData(UART_3_INST);
            uint16_t head = g_crsf_rx_head;
            uint16_t next_head = board_crsf_uart_next_index(head);

            if (next_head == g_crsf_rx_tail) {
                ++g_crsf_rx_overflow_count;
            } else {
                g_crsf_rx_buffer[head] = byte;
                __DMB();
                g_crsf_rx_head = next_head;
            }
        }
    }
}

bool board_crsf_uart_read_byte(uint8_t *byte)
{
    uint16_t tail;

    if (byte == NULL) {
        return false;
    }

    tail = g_crsf_rx_tail;
    if (tail == g_crsf_rx_head) {
        return false;
    }

    *byte = g_crsf_rx_buffer[tail];
    __DMB();
    g_crsf_rx_tail = board_crsf_uart_next_index(tail);
    return true;
}

uint32_t board_crsf_uart_rx_overflow_count(void)
{
    return g_crsf_rx_overflow_count;
}
