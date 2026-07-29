#ifndef APP_CHASSIS_SERVICE_H
#define APP_CHASSIS_SERVICE_H

#include <stdint.h>

void h723_chassis_service_init(void);
void h723_chassis_service_step(uint32_t now_ms);
void h723_chassis_on_uart7_rx_event(uint16_t size);
void h723_chassis_on_uart7_error(void);
void h723_chassis_on_fdcan1_rx(void);
void h723_chassis_on_fdcan2_rx(void);

#endif
