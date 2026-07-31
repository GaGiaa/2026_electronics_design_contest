#include <assert.h>
#include <stdbool.h>

#include "app_telemetry_tx_guard.h"

static void test_completion_during_dma_start_does_not_leave_guard_busy(void)
{
    app_telemetry_tx_guard_t guard = {0};

    assert(app_telemetry_tx_guard_reserve(&guard, 10U));
    app_telemetry_tx_guard_complete(&guard);
    assert(!app_telemetry_tx_guard_is_busy(&guard));
}

static void test_failed_dma_start_releases_guard(void)
{
    app_telemetry_tx_guard_t guard = {0};

    assert(app_telemetry_tx_guard_reserve(&guard, 10U));
    app_telemetry_tx_guard_start_failed(&guard);
    assert(!app_telemetry_tx_guard_is_busy(&guard));
}

static void test_pending_transfer_times_out_at_configured_deadline(void)
{
    app_telemetry_tx_guard_t guard = {0};

    assert(app_telemetry_tx_guard_reserve(&guard, 10U));
    assert(!app_telemetry_tx_guard_timed_out(&guard, 29U, 20U));
    assert(app_telemetry_tx_guard_timed_out(&guard, 30U, 20U));
}

int main(void)
{
    test_completion_during_dma_start_does_not_leave_guard_busy();
    test_failed_dma_start_releases_guard();
    test_pending_transfer_times_out_at_configured_deadline();
    return 0;
}
