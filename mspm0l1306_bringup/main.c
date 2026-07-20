#include <stdbool.h>
#include <stdint.h>

#include "board_led.h"
#include "board_uart.h"
#include "echo_queue.h"
#include "ti_msp_dl_config.h"

#ifndef LED_TOGGLE_INTERVAL_MS
#define LED_TOGGLE_INTERVAL_MS 2000U
#endif

static EchoQueue g_echo_queue;
static volatile uint32_t g_milliseconds;

void SysTick_Handler(void)
{
    ++g_milliseconds;
}

void UART_0_INST_IRQHandler(void)
{
    board_uart_irq_handler();
}

static bool app_try_get_echo_byte(uint8_t *byte)
{
    bool has_byte;

    __disable_irq();
    has_byte = echo_queue_pop(&g_echo_queue, byte);
    __enable_irq();
    return has_byte;
}

int main(void)
{
    uint8_t byte;
    uint32_t last_toggle_ms = 0U;

    SYSCFG_DL_init();
    echo_queue_init(&g_echo_queue);
    board_uart_enable_rx_interrupt(&g_echo_queue);
    (void) SysTick_Config(CPUCLK_FREQ / 1000U);

    while (true) {
        if (!board_uart_tx_full() && app_try_get_echo_byte(&byte)) {
            board_uart_transmit(byte);
        }

        if ((uint32_t) (g_milliseconds - last_toggle_ms) >= LED_TOGGLE_INTERVAL_MS) {
            last_toggle_ms += LED_TOGGLE_INTERVAL_MS;
            board_led_toggle();
        }
    }
}
