#ifndef APP_TELEMETRY_TX_GUARD_H
#define APP_TELEMETRY_TX_GUARD_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool busy;
    uint32_t started_ms;
} app_telemetry_tx_guard_t;

void app_telemetry_tx_guard_init(volatile app_telemetry_tx_guard_t *guard);
bool app_telemetry_tx_guard_reserve(volatile app_telemetry_tx_guard_t *guard,
                                    uint32_t now_ms);
void app_telemetry_tx_guard_complete(volatile app_telemetry_tx_guard_t *guard);
void app_telemetry_tx_guard_start_failed(volatile app_telemetry_tx_guard_t *guard);
bool app_telemetry_tx_guard_is_busy(const volatile app_telemetry_tx_guard_t *guard);
bool app_telemetry_tx_guard_timed_out(const volatile app_telemetry_tx_guard_t *guard,
                                      uint32_t now_ms,
                                      uint32_t timeout_ms);

#endif
