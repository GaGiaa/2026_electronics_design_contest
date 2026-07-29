#ifndef APP_JY901S_SERVICE_H
#define APP_JY901S_SERVICE_H

#include <stdint.h>

void h723_jy901s_service_init(void);
void h723_jy901s_service_step(uint32_t now_ms);
void h723_jy901s_on_uart9_rx_event(uint16_t size);
void h723_jy901s_on_uart9_error(void);

#endif
