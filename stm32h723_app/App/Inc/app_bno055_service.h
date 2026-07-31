#ifndef APP_BNO055_SERVICE_H
#define APP_BNO055_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "app_bno055.h"

void h723_bno055_service_init(void);
void h723_bno055_service_step(uint32_t now_ms);
bool h723_bno055_service_get_snapshot(app_bno055_snapshot_t *snapshot,
                                      uint32_t *sample_age_ms);

#endif /* APP_BNO055_SERVICE_H */
