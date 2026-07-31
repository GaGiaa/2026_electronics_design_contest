#ifndef APP_JY901S_SERVICE_H
#define APP_JY901S_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float vehicle_pitch_deg;
    uint32_t complete_sample_count;
    uint32_t sample_time_ms;
    bool sample_valid;
    bool calibration_valid;
} h723_jy901s_control_snapshot_t;

void h723_jy901s_service_init(void);
void h723_jy901s_service_step(uint32_t now_ms);
void h723_jy901s_on_uart9_rx_event(uint16_t size);
void h723_jy901s_on_uart9_error(void);
bool h723_jy901s_service_get_snapshot(h723_jy901s_control_snapshot_t *snapshot,
                                      uint32_t now_ms, uint32_t *sample_age_ms);

#endif
