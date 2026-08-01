#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "app_pipe_startup.h"

static app_pipe_startup_input_t make_input(void)
{
    return (app_pipe_startup_input_t){
        .balance_zero_valid = false,
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

static void test_balance_home_enters_calibration_and_keeps_id3_disabled(void)
{
    app_pipe_startup_t startup;
    app_pipe_startup_input_t input = make_input();
    app_pipe_startup_snapshot_t snapshot;

    app_pipe_startup_init(&startup);
    input.balance_zero_valid = true;
    app_pipe_startup_step(&startup, &input, &snapshot);

    assert(snapshot.state == APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED);
    assert(!snapshot.calibration_valid);
    assert(!snapshot.id3_allowed);
    assert(snapshot.other_motors_allowed);
}

static void test_capture_rejects_invalid_or_stale_imu(void)
{
    app_pipe_startup_t startup;
    app_pipe_startup_input_t input = make_input();
    app_pipe_startup_snapshot_t snapshot;

    app_pipe_startup_init(&startup);
    input.balance_zero_valid = true;
    app_pipe_startup_step(&startup, &input, &snapshot);

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
    input.balance_zero_valid = true;
    app_pipe_startup_step(&startup, &input, &snapshot);
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
    input.balance_zero_valid = true;
    app_pipe_startup_step(&startup, &input, &snapshot);
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
    input.balance_zero_valid = true;
    app_pipe_startup_step(&startup, &input, &snapshot);
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
    input.balance_zero_valid = true;
    app_pipe_startup_step(&startup, &input, &snapshot);
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
    test_balance_home_enters_calibration_and_keeps_id3_disabled();
    test_capture_rejects_invalid_or_stale_imu();
    test_capture_rising_edge_records_fresh_pitch_and_enters_ready();
    test_capture_requires_a_new_rising_edge();
    test_skip_locks_only_id3_and_pa6_is_ignored();
    test_ready_returns_to_home_gate_when_balance_zero_is_lost();
    return 0;
}
