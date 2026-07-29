#ifndef APP_TIME_H
#define APP_TIME_H

#include <stdint.h>

/* Returns the FreeRTOS kernel tick in milliseconds (configTICK_RATE_HZ = 1000). */
uint32_t h723_app_time_now_ms(void);

#endif
