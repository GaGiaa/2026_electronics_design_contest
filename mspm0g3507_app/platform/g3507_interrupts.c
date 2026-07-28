#include "platform/g3507_interrupts.h"

#include "drivers/crsf_uart/board_crsf_uart.h"
#include "drivers/encoder/board_encoder.h"
#include "drivers/hcsr04/board_hcsr04.h"
#include "config/app_config.h"
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

void GROUP1_IRQHandler(void)
{
    switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
    case ENCODER_GPIOA_INT_IIDX:
        board_encoder_gpioa_irq_handler();
        break;
    case GPIO_MULTIPLE_GPIOB_INT_IIDX:
#if APP_HCSR04_ENABLE
        board_hcsr04_gpio_irq_handler();
#endif
        board_encoder_gpiob_irq_handler();
        break;
    default:
        break;
    }
}
