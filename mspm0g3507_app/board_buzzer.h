#ifndef BOARD_BUZZER_H
#define BOARD_BUZZER_H

#include <stdint.h>

void board_buzzer_init(uint32_t frequency_hz, uint8_t duty_percent);
void board_buzzer_start(void);
void board_buzzer_stop(void);

#endif
