#ifndef APP_BUZZER_H
#define APP_BUZZER_H

#include <stdint.h>

void h723_buzzer_service_init(void);
void h723_buzzer_service_step(uint32_t now_ms);

#endif
