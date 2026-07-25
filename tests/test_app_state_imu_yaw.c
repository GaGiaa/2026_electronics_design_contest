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
    app_drive_control_snapshot_t drive = {0};
    app_drive_control_snapshot_t copied_drive = {0};

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

    drive.mode = CRSF_DRIVE_MODE_LINE_TRACKING;
    drive.link_active = true;
    drive.sb_raw = 1500U;
    drive.line_error = -250;
    drive.line_strength = 2048U;
    drive.line_valid = true;
    drive.base_speed_mm_per_s = 300.0f;
    drive.turn_speed_mm_per_s = 20.0f;
    drive.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT] = 320.0f;
    drive.wheel_feedback_mm_per_s[BOARD_MOTOR_FRONT_LEFT] = 315.0f;
    drive.line_sequence = 7U;
    drive.control_sequence = 11U;
    app_state_drive_control_publish(&drive);
    app_state_drive_control_snapshot_copy(&copied_drive);
    assert(copied_drive.mode == CRSF_DRIVE_MODE_LINE_TRACKING);
    assert(copied_drive.link_active);
    assert(copied_drive.sb_raw == 1500U);
    assert(copied_drive.line_error == -250);
    assert_float_equal(copied_drive.base_speed_mm_per_s, 300.0f);
    assert_float_equal(copied_drive.wheel_targets_mm_per_s[
                           BOARD_MOTOR_FRONT_LEFT], 320.0f);
    assert_float_equal(copied_drive.wheel_feedback_mm_per_s[
                           BOARD_MOTOR_FRONT_LEFT], 315.0f);
    assert(copied_drive.line_sequence == 7U);
    assert(copied_drive.control_sequence == 11U);

    app_state_imu_yaw_invalidate();
    app_state_imu_yaw_snapshot_copy(&snapshot);
    assert(!snapshot.valid);
    assert(snapshot.sequence == 2U);

    return 0;
}
