#include <stdint.h>
#include <stdio.h>

#include "services/rtos_monitor/rtos_monitor.h"

static int expect_u32(const char *name, uint32_t actual, uint32_t expected)
{
    if (actual != expected) {
        fprintf(stderr, "%s: expected %lu, got %lu\n",
                name, (unsigned long)expected, (unsigned long)actual);
        return 0;
    }
    return 1;
}

int main(void)
{
    int passed = 1;

    passed &= expect_u32("counter delta", rtos_monitor_counter_delta(150U, 100U), 50U);
    passed &= expect_u32("counter wrap delta",
                         rtos_monitor_counter_delta(5U, UINT32_MAX - 4U), 10U);
    passed &= expect_u32("percentage", rtos_monitor_percent_x100(250U, 1000U), 2500U);
    passed &= expect_u32("percentage clamps", rtos_monitor_percent_x100(1200U, 1000U), 10000U);
    passed &= expect_u32("zero percentage", rtos_monitor_percent_x100(1U, 0U), 0U);

    return passed ? 0 : 1;
}
