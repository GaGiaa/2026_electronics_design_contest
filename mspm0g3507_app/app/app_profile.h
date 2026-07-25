#ifndef APP_PROFILE_H
#define APP_PROFILE_H

#include <stdbool.h>

typedef struct {
    bool enable_motor_control;
    bool enable_imu;
    bool enable_grayscale;
    bool enable_crsf;
    bool enable_oled;
    bool enable_telemetry;
} app_profile_t;

const app_profile_t *app_profile_get(void);

#endif
