#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "app_pipe_startup.h"

static app_pipe_startup_input_t make_input(void)
{
    return (app_pipe_startup_input_t){
        .balance_zero_valid = false,
        .id3_feedback_valid = false,
        .id3_position_deg = 0.0f,
        .id3_output_speed_rpm = 0.0f,
        .now_ms = 0U,
        .imu_pitch_valid = false,
        .imu_pitch_fresh = false,
        .imu_pitch_deg = 0.0f,
        .buttons = 0U,
    };
}

static void test_wait_home_only_disables_id3(void)
{
    app_pipe_startup_t startup;
    app_pipe_startup_input_t input = make_input();
    app_pipe_startup_snapshot_t snapshot;

    app_pipe_startup_init(&startup);
    app_pipe_startup_step(&startup, &input, &snapshot);

    assert(snapshot.state == APP_PIPE_STARTUP_STATE_WAIT_HOME);
    assert(!snapshot.calibration_valid);
    assert(!snapshot.id3_allowed);
    assert(snapshot.other_motors_allowed);
}

static void enter_move_to_calibration_position(app_pipe_startup_t *startup,
                                               app_pipe_startup_input_t *input,
                                               app_pipe_startup_snapshot_t *snapshot)
{
    input->balance_zero_valid = true;
    input->id3_feedback_valid = true;
    app_pipe_startup_step(startup, input, snapshot);
    assert(snapshot->state == APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION);
    assert(snapshot->id3_allowed);
    assert(fabsf(snapshot->calibration_target_position_deg - 134.0f) < 0.0001f);
}

static void reach_calibration_page(app_pipe_startup_t *startup,
                                   app_pipe_startup_input_t *input,
                                   app_pipe_startup_snapshot_t *snapshot)
{
    enter_move_to_calibration_position(startup, input, snapshot);
    input->id3_position_deg = 134.0f;
    input->id3_output_speed_rpm = 0.0f;
    input->now_ms = 100U;
    app_pipe_startup_step(startup, input, snapshot);
    input->now_ms = 300U;
    app_pipe_startup_step(startup, input, snapshot);
    assert(snapshot->state == APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED);
    assert(!snapshot->id3_allowed);
}

static void test_balance_home_enters_move_before_calibration(void)
{
    app_pipe_startup_t startup;
    app_pipe_startup_input_t input = make_input();
    app_pipe_startup_snapshot_t snapshot;

    app_pipe_startup_init(&startup);
    enter_move_to_calibration_position(&startup, &input, &snapshot);

    assert(snapshot.state == APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION);
    assert(!snapshot.calibration_valid);
    assert(snapshot.id3_allowed);
    assert(snapshot.other_motors_allowed);
}

static void test_move_requires_position_and_speed_to_remain_stable(void)
{
    app_pipe_startup_t startup;
    app_pipe_startup_input_t input = make_input();
    app_pipe_startup_snapshot_t snapshot;

    app_pipe_startup_init(&startup);
    enter_move_to_calibration_position(&startup, &input, &snapshot);
    input.id3_position_deg = 133.5f;
    input.id3_output_speed_rpm = 6.0f;
    input.now_ms = 100U;
    app_pipe_startup_step(&startup, &input, &snapshot);
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION);

    input.id3_position_deg = 134.0f;
    input.id3_output_speed_rpm = 4.0f;
    app_pipe_startup_step(&startup, &input, &snapshot);
    input.now_ms = 299U;
    app_pipe_startup_step(&startup, &input, &snapshot);
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION);
    input.now_ms = 300U;
    app_pipe_startup_step(&startup, &input, &snapshot);
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED);
    assert(!snapshot.id3_allowed);
}

static void test_move_timeout_or_feedback_loss_locks_only_id3(void)
{
    app_pipe_startup_t startup;
    app_pipe_startup_input_t input = make_input();
    app_pipe_startup_snapshot_t snapshot;

    app_pipe_startup_init(&startup);
    enter_move_to_calibration_position(&startup, &input, &snapshot);
    input.now_ms = 10001U;
    app_pipe_startup_step(&startup, &input, &snapshot);
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_ID3_LOCKED);
    assert(!snapshot.id3_allowed);
    assert(snapshot.other_motors_allowed);

    app_pipe_startup_init(&startup);
    input = make_input();
    enter_move_to_calibration_position(&startup, &input, &snapshot);
    input.id3_feedback_valid = false;
    app_pipe_startup_step(&startup, &input, &snapshot);
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_ID3_LOCKED);
    assert(snapshot.other_motors_allowed);
}

static void test_capture_rejects_invalid_or_stale_imu(void)
{
    app_pipe_startup_t startup;
    app_pipe_startup_input_t input = make_input();
    app_pipe_startup_snapshot_t snapshot;

    app_pipe_startup_init(&startup);
    reach_calibration_page(&startup, &input, &snapshot);

    input.buttons = APP_PIPE_STARTUP_BUTTON_CAPTURE;
    input.imu_pitch_valid = false;
    input.imu_pitch_fresh = true;
    input.imu_pitch_deg = 12.0f;
    app_pipe_startup_step(&startup, &input, &snapshot);
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED);
    assert(!snapshot.calibration_valid);

    input.buttons = 0U;
    app_pipe_startup_step(&startup, &input, &snapshot);
    input.buttons = APP_PIPE_STARTUP_BUTTON_CAPTURE;
    input.imu_pitch_valid = true;
    input.imu_pitch_fresh = false;
    app_pipe_startup_step(&startup, &input, &snapshot);
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED);
    assert(!snapshot.calibration_valid);
    assert(!snapshot.id3_allowed);
}

static void test_capture_rising_edge_records_fresh_pitch_and_enters_ready(void)
{
    app_pipe_startup_t startup;
    app_pipe_startup_input_t input = make_input();
    app_pipe_startup_snapshot_t snapshot;

    app_pipe_startup_init(&startup);
    reach_calibration_page(&startup, &input, &snapshot);
    input.buttons = APP_PIPE_STARTUP_BUTTON_CAPTURE;
    input.imu_pitch_valid = true;
    input.imu_pitch_fresh = true;
    input.imu_pitch_deg = -3.25f;
    app_pipe_startup_step(&startup, &input, &snapshot);

    assert(snapshot.state == APP_PIPE_STARTUP_STATE_READY);
    assert(snapshot.calibration_valid);
    assert(fabsf(snapshot.captured_pitch_deg + 3.25f) < 0.0001f);
    assert(snapshot.id3_allowed);
    assert(snapshot.other_motors_allowed);
}

static void test_capture_requires_a_new_rising_edge(void)
{
    app_pipe_startup_t startup;
    app_pipe_startup_input_t input = make_input();
    app_pipe_startup_snapshot_t snapshot;

    app_pipe_startup_init(&startup);
    reach_calibration_page(&startup, &input, &snapshot);
    input.buttons = APP_PIPE_STARTUP_BUTTON_CAPTURE;
    input.imu_pitch_valid = false;
    input.imu_pitch_fresh = true;
    app_pipe_startup_step(&startup, &input, &snapshot);

    input.imu_pitch_valid = true;
    input.imu_pitch_deg = 8.0f;
    app_pipe_startup_step(&startup, &input, &snapshot);
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED);
    assert(!snapshot.calibration_valid);

    input.buttons = 0U;
    app_pipe_startup_step(&startup, &input, &snapshot);
    input.buttons = APP_PIPE_STARTUP_BUTTON_CAPTURE;
    app_pipe_startup_step(&startup, &input, &snapshot);
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_READY);
    assert(fabsf(snapshot.captured_pitch_deg - 8.0f) < 0.0001f);
}

static void test_skip_locks_only_id3_and_pa6_is_ignored(void)
{
    app_pipe_startup_t startup;
    app_pipe_startup_input_t input = make_input();
    app_pipe_startup_snapshot_t snapshot;

    app_pipe_startup_init(&startup);
    reach_calibration_page(&startup, &input, &snapshot);
    input.buttons = APP_PIPE_STARTUP_BUTTON_PA6;
    app_pipe_startup_step(&startup, &input, &snapshot);
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED);

    input.buttons = 0U;
    app_pipe_startup_step(&startup, &input, &snapshot);
    input.buttons = APP_PIPE_STARTUP_BUTTON_SKIP;
    app_pipe_startup_step(&startup, &input, &snapshot);
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_ID3_LOCKED);
    assert(!snapshot.calibration_valid);
    assert(!snapshot.id3_allowed);
    assert(snapshot.other_motors_allowed);
}

static void test_ready_returns_to_home_gate_when_balance_zero_is_lost(void)
{
    app_pipe_startup_t startup;
    app_pipe_startup_input_t input = make_input();
    app_pipe_startup_snapshot_t snapshot;

    app_pipe_startup_init(&startup);
    reach_calibration_page(&startup, &input, &snapshot);
    input.buttons = APP_PIPE_STARTUP_BUTTON_CAPTURE;
    input.imu_pitch_valid = true;
    input.imu_pitch_fresh = true;
    app_pipe_startup_step(&startup, &input, &snapshot);
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_READY);

    input.buttons = 0U;
    input.balance_zero_valid = false;
    app_pipe_startup_step(&startup, &input, &snapshot);
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_WAIT_HOME);
    assert(!snapshot.id3_allowed);
}

int main(void)
{
    test_wait_home_only_disables_id3();
    test_balance_home_enters_move_before_calibration();
    test_move_requires_position_and_speed_to_remain_stable();
    test_move_timeout_or_feedback_loss_locks_only_id3();
    test_capture_rejects_invalid_or_stale_imu();
    test_capture_rising_edge_records_fresh_pitch_and_enters_ready();
    test_capture_requires_a_new_rising_edge();
    test_skip_locks_only_id3_and_pa6_is_ignored();
    test_ready_returns_to_home_gate_when_balance_zero_is_lost();
    return 0;
}
