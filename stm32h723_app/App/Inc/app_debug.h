#ifndef APP_DEBUG_H
#define APP_DEBUG_H

#include <stdint.h>

typedef struct {
    uint32_t boot_count;
    uint32_t uptime_ms;
    uint32_t task_loop_count;
    uint32_t telemetry_enabled;
    uint32_t tx_start_count;
    uint32_t tx_complete_count;
    uint32_t tx_drop_count;
    uint32_t last_hal_status;
    uint32_t tx_in_flight;
} h723_debug_t;

extern volatile h723_debug_t g_h723_debug;

#endif
