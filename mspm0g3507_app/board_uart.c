#include "board_uart.h"

#include <FreeRTOS.h>
#include <task.h>

#include "ti_msp_dl_config.h"

static QueueHandle_t g_rx_queue;
static volatile uint32_t g_rx_overflow_count;
#define BOARD_UART_TX_QUEUE_LENGTH 8U

typedef struct {
    uint16_t length;
    uint8_t data[BOARD_UART_TX_MESSAGE_MAX_LENGTH];
} board_uart_tx_message_t;

static StaticQueue_t g_tx_queue_buffer;
static uint8_t g_tx_queue_storage[BOARD_UART_TX_QUEUE_LENGTH *
                                  sizeof(board_uart_tx_message_t)];
static QueueHandle_t g_tx_queue;

void board_uart_enable_rx_interrupt(QueueHandle_t queue)
{
    g_rx_queue = queue;
    g_tx_queue = xQueueCreateStatic(BOARD_UART_TX_QUEUE_LENGTH,
                                    sizeof(board_uart_tx_message_t),
                                    g_tx_queue_storage, &g_tx_queue_buffer);
    configASSERT(g_tx_queue != NULL);
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
void board_uart_write(const uint8_t *data, size_t length)
{
    board_uart_tx_message_t message;
    size_t index;

    if ((data == NULL) || (length == 0U) ||
        (length > BOARD_UART_TX_MESSAGE_MAX_LENGTH) || (g_tx_queue == NULL)) {
        return;
    }
    message.length = (uint16_t)length;
    for (index = 0U; index < length; ++index) {
        message.data[index] = data[index];
    }
    (void)xQueueSend(g_tx_queue, &message, portMAX_DELAY);
}

void board_uart_tx_task(void *argument)
{
    board_uart_tx_message_t message;
    size_t index;

    (void)argument;
    for (;;) {
        if (xQueueReceive(g_tx_queue, &message, portMAX_DELAY) == pdPASS) {
            for (index = 0U; index < message.length; ++index) {
                while (board_uart_tx_full()) { taskYIELD(); }
                board_uart_transmit(message.data[index]);
            }
        }
    }
}
uint32_t board_uart_rx_overflow_count(void) { return g_rx_overflow_count; }
