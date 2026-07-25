#ifndef CRSF_CONTROL_H
#define CRSF_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "drivers/motor/board_motor.h"
#include "config/crsf_config.h"
#include "protocols/crsf/crsf_protocol.h"

typedef struct {
    bool valid;
    uint16_t channels[CRSF_CHANNEL_COUNT];
    uint32_t last_valid_time_ms;
} crsf_control_input_t;

bool crsf_control_mix(const crsf_control_input_t *input,
                      uint32_t now_ms,
                      float targets[BOARD_MOTOR_COUNT]);

#endif
