#include "app_telemetry_tx_guard.h"

#include <stddef.h>

void app_telemetry_tx_guard_init(volatile app_telemetry_tx_guard_t *guard)
{
    if (guard == NULL) {
        return;
    }
    guard->busy = false;
    guard->started_ms = 0U;
}

bool app_telemetry_tx_guard_reserve(volatile app_telemetry_tx_guard_t *guard,
                                    uint32_t now_ms)
{
    if (guard == NULL || guard->busy) {
        return false;
    }
    guard->started_ms = now_ms;
    guard->busy = true;
    return true;
}

void app_telemetry_tx_guard_complete(volatile app_telemetry_tx_guard_t *guard)
{
    if (guard != NULL) {
        guard->busy = false;
    }
}

void app_telemetry_tx_guard_start_failed(volatile app_telemetry_tx_guard_t *guard)
{
    app_telemetry_tx_guard_complete(guard);
}

bool app_telemetry_tx_guard_is_busy(const volatile app_telemetry_tx_guard_t *guard)
{
    return guard != NULL && guard->busy;
}

bool app_telemetry_tx_guard_timed_out(const volatile app_telemetry_tx_guard_t *guard,
                                      uint32_t now_ms,
                                      uint32_t timeout_ms)
{
    return guard != NULL && guard->busy && timeout_ms > 0U &&
           (uint32_t)(now_ms - guard->started_ms) >= timeout_ms;
}
