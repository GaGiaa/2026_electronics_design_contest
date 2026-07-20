#include "board_led.h"

#include "ti_msp_dl_config.h"

void board_led_toggle(void)
{
    DL_GPIO_togglePins(GPIO_LED_PORT, GPIO_LED_LED_PIN);
}
