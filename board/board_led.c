#include "board_led.h"

#include "ti_msp_dl_config.h"

void board_led_toggle(void)
{
    DL_GPIO_togglePins(GPIO_LED4_PORT, GPIO_LED4_LED_PIN);
}
