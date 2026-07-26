#include "platform/g3507_interrupts.h"

#include "config/app_config.h"
#if APP_BLUETOOTH_UART_ENABLE
#include "drivers/bluetooth_uart/board_bluetooth_uart.h"
#endif
#include "drivers/crsf_uart/board_crsf_uart.h"
#include "drivers/encoder/board_encoder.h"
#include "drivers/uart/board_uart.h"
#include "ti_msp_dl_config.h"

void UART_0_INST_IRQHandler(void)
{
    board_uart_irq_handler();
}

void UART_3_INST_IRQHandler(void)
{
    board_crsf_uart_irq_handler();
}

void UART_2_INST_IRQHandler(void)
{
#if APP_BLUETOOTH_UART_ENABLE
    board_bluetooth_uart_irq_handler();
#endif
}

void GROUP1_IRQHandler(void)
{
    switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
    case ENCODER_GPIOA_INT_IIDX:
        board_encoder_gpioa_irq_handler();
        break;
    case ENCODER_GPIOB_INT_IIDX:
        board_encoder_gpiob_irq_handler();
        break;
    default:
        break;
    }
}
