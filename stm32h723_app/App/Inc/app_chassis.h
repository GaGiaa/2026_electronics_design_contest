#ifndef APP_CHASSIS_H
#define APP_CHASSIS_H

#include <stdbool.h>
#include <stdint.h>

#include "app_crsf.h"
#include "app_config.h"
#include "pid.h"

typedef struct {
    bool manual_active;
    float forward_normalized;
    float turn_normalized;
    float left_target_rpm;
    float right_target_rpm;
} app_chassis_command_t;

void app_chassis_mix(const app_crsf_input_t *input, uint32_t now_ms, app_chassis_command_t *command);

#endif
