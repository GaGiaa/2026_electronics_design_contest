#include <assert.h>
#include <math.h>

#include "app/app_state.h"

volatile motor_control_wheel_status_t
    g_motor_control_status[BOARD_MOTOR_COUNT];

static void assert_float_equal(float actual, float expected)
{
    assert(fabsf(actual - expected) < 0.0001f);
}

int main(void)
{
    app_imu_yaw_snapshot_t snapshot;

    app_state_init();
    app_state_imu_yaw_snapshot_copy(&snapshot);
    assert(!snapshot.valid);
    assert(snapshot.sequence == 0U);

    app_state_imu_yaw_publish(12.5f, -3.25f, 0.75f);
    app_state_imu_yaw_snapshot_copy(&snapshot);
    assert(snapshot.valid);
    assert(snapshot.sequence == 1U);
    assert_float_equal(snapshot.yaw_deg, 12.5f);
    assert_float_equal(snapshot.yaw_rate_dps, -3.25f);
    assert_float_equal(snapshot.gyro_bias_z_dps, 0.75f);

    app_state_imu_yaw_invalidate();
    app_state_imu_yaw_snapshot_copy(&snapshot);
    assert(!snapshot.valid);
    assert(snapshot.sequence == 2U);

    return 0;
}
