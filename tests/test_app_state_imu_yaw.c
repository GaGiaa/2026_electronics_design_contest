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
    app_button_snapshot_t buttons;
    app_imu_fusion_snapshot_t snapshot;
    app_drive_control_snapshot_t drive = {0};
    app_drive_control_snapshot_t copied_drive = {0};

    app_state_init();
    assert(g_encoder_sample_sequence == 0U);
    assert(g_grayscale_publish_sequence == 0U);
    assert(g_drive_control_publish_sequence == 0U);
    assert(g_button_publish_sequence == 0U);
    assert(g_imu_fusion_publish_sequence == 0U);
    assert(!g_imu_fusion_snapshot.valid);
    assert(g_imu_debug.chip_id == 0U);
    assert(!g_imu_debug.initialized);
    assert(g_imu_debug.init_status == BOARD_BMI160_STATUS_OK);
    assert(g_imu_debug.last_read_status == BOARD_BMI160_STATUS_OK);
    assert(g_imu_debug.init_attempts == 0U);
    assert(g_imu_debug.sample_attempts == 0U);
    assert(g_imu_debug.sample_successes == 0U);
    assert(g_imu_debug.last_sample.accel_z == 0);
    assert(g_imu_debug.sequence == 0U);

    app_state_buttons_snapshot_copy(&buttons);
    assert(buttons.pressed_mask == 0U);
    assert(buttons.pressed_edge_mask == 0U);
    assert(buttons.released_edge_mask == 0U);
    assert(buttons.sequence == 0U);

    app_state_buttons_publish(0x05U, 0x01U, 0x02U);
    app_state_buttons_snapshot_copy(&buttons);
    assert(buttons.pressed_mask == 0x05U);
    assert(buttons.pressed_edge_mask == 0x01U);
    assert(buttons.released_edge_mask == 0x02U);
    assert(buttons.sequence == 1U);
    assert(g_button_publish_sequence == 2U);

    app_state_encoder_cycle_begin();
    assert(g_encoder_sample_sequence == 1U);
    app_state_encoder_cycle_end();
    assert(g_encoder_sample_sequence == 2U);

    app_state_imu_fusion_snapshot_copy(&snapshot);
    assert(!snapshot.valid);
    assert(snapshot.sequence == 0U);

    snapshot = (app_imu_fusion_snapshot_t){
        .yaw_deg = 12.5f,
        .yaw_rate_dps = -3.25f,
        .roll_deg = 4.5f,
        .pitch_deg = -2.25f,
        .accel_norm_g = 1.08f,
        .gyro_bias_x_dps = 0.10f,
        .gyro_bias_y_dps = -0.20f,
        .gyro_bias_z_dps = 0.75f,
        .dt_s = 0.005f,
        .acceleration_valid = true,
        .stationary_confirmed = true,
        .calibrated = true,
        .valid = true,
    };
    app_state_imu_fusion_publish(&snapshot);
    app_state_imu_fusion_snapshot_copy(&snapshot);
    assert(snapshot.valid);
    assert(snapshot.sequence == 1U);
    assert_float_equal(snapshot.yaw_deg, 12.5f);
    assert_float_equal(snapshot.yaw_rate_dps, -3.25f);
    assert_float_equal(snapshot.roll_deg, 4.5f);
    assert_float_equal(snapshot.pitch_deg, -2.25f);
    assert_float_equal(snapshot.accel_norm_g, 1.08f);
    assert(snapshot.acceleration_valid);
    assert_float_equal(snapshot.gyro_bias_x_dps, 0.10f);
    assert_float_equal(snapshot.gyro_bias_y_dps, -0.20f);
    assert_float_equal(snapshot.gyro_bias_z_dps, 0.75f);
    assert(snapshot.stationary_confirmed);
    assert(snapshot.calibrated);
    assert_float_equal(snapshot.dt_s, 0.005f);
    assert(g_imu_fusion_publish_sequence == 2U);
    assert(g_imu_fusion_snapshot.valid);

    board_grayscale_snapshot_t grayscale = {0};
    grayscale.sequence = 9U;
    app_state_grayscale_publish(&grayscale);
    assert(g_grayscale_publish_sequence == 2U);

    drive.mode = CRSF_DRIVE_MODE_LINE_TRACKING;
    drive.link_active = true;
    drive.sb_raw = 1500U;
    drive.sc_raw = 992U;
    drive.line_error = -250;
    drive.line_strength = 2048U;
    drive.line_valid = true;
    drive.base_speed_mm_per_s = 300.0f;
    drive.turn_speed_mm_per_s = 20.0f;
    drive.yaw_valid = true;
    drive.yaw_target_deg = -90.0f;
    drive.yaw_feedback_deg = -85.0f;
    drive.yaw_error_deg = -5.0f;
    drive.course_heading_hold = true;
    drive.course_heading_target_deg = 175.0f;
    drive.wheel_targets_mm_per_s[BOARD_MOTOR_FRONT_LEFT] = 320.0f;
    drive.wheel_feedback_mm_per_s[BOARD_MOTOR_FRONT_LEFT] = 315.0f;
    drive.line_sequence = 7U;
    drive.control_sequence = 11U;
    app_state_drive_control_publish(&drive);
    assert(g_drive_control_publish_sequence == 2U);
    app_state_drive_control_snapshot_copy(&copied_drive);
    assert(copied_drive.mode == CRSF_DRIVE_MODE_LINE_TRACKING);
    assert(copied_drive.link_active);
    assert(copied_drive.sb_raw == 1500U);
    assert(copied_drive.sc_raw == 992U);
    assert(copied_drive.line_error == -250);
    assert_float_equal(copied_drive.base_speed_mm_per_s, 300.0f);
    assert_float_equal(copied_drive.wheel_targets_mm_per_s[
                           BOARD_MOTOR_FRONT_LEFT], 320.0f);
    assert_float_equal(copied_drive.wheel_feedback_mm_per_s[
                           BOARD_MOTOR_FRONT_LEFT], 315.0f);
    assert(copied_drive.yaw_valid);
    assert_float_equal(copied_drive.yaw_target_deg, -90.0f);
    assert_float_equal(copied_drive.yaw_feedback_deg, -85.0f);
    assert_float_equal(copied_drive.yaw_error_deg, -5.0f);
    assert(copied_drive.course_heading_hold);
    assert_float_equal(copied_drive.course_heading_target_deg, 175.0f);
    assert(copied_drive.line_sequence == 7U);
    assert(copied_drive.control_sequence == 11U);

    app_state_imu_fusion_invalidate();
    app_state_imu_fusion_snapshot_copy(&snapshot);
    assert(!snapshot.valid);
    assert(snapshot.sequence == 2U);

    return 0;
}
