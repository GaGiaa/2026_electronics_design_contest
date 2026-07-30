#ifndef APP_BNO055_SERVICE_H
#define APP_BNO055_SERVICE_H

#include <stdint.h>

void h723_bno055_service_init(void);
void h723_bno055_service_step(uint32_t now_ms);

#endif /* APP_BNO055_SERVICE_H */
