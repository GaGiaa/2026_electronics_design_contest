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
    };
}

static void enter_move_to_startup_position(app_pipe_startup_t *startup,
                                           app_pipe_startup_input_t *input,
                                           app_pipe_startup_snapshot_t *snapshot)
{
    input->balance_zero_valid = true;
    input->id3_feedback_valid = true;
    app_pipe_startup_step(startup, input, snapshot);
    assert(snapshot->state == APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION);
    assert(snapshot->id3_allowed);
    assert(fabsf(snapshot->startup_target_position_deg - 134.0f) < 0.0001f);
}

static void reach_ready(app_pipe_startup_t *startup,
                        app_pipe_startup_input_t *input,
                        app_pipe_startup_snapshot_t *snapshot)
{
    enter_move_to_startup_position(startup, input, snapshot);
    input->id3_position_deg = 134.0f;
    input->id3_output_speed_rpm = 0.0f;
    input->now_ms = 100U;
    app_pipe_startup_step(startup, input, snapshot);
    input->now_ms = 300U;
    app_pipe_startup_step(startup, input, snapshot);
    assert(snapshot->state == APP_PIPE_STARTUP_STATE_READY);
    assert(snapshot->id3_allowed);
}

static void test_wait_home_only_disables_id3(void)
{
    app_pipe_startup_t startup;
    app_pipe_startup_input_t input = make_input();
    app_pipe_startup_snapshot_t snapshot;

    app_pipe_startup_init(&startup);
    app_pipe_startup_step(&startup, &input, &snapshot);

    assert(snapshot.state == APP_PIPE_STARTUP_STATE_WAIT_HOME);
    assert(!snapshot.id3_allowed);
    assert(snapshot.other_motors_allowed);
}

static void test_home_enters_move_before_ready(void)
{
    app_pipe_startup_t startup;
    app_pipe_startup_input_t input = make_input();
    app_pipe_startup_snapshot_t snapshot;

    app_pipe_startup_init(&startup);
    enter_move_to_startup_position(&startup, &input, &snapshot);

    assert(snapshot.state == APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION);
    assert(snapshot.id3_allowed);
    assert(snapshot.other_motors_allowed);
}

static void test_move_requires_position_and_speed_to_remain_stable(void)
{
    app_pipe_startup_t startup;
    app_pipe_startup_input_t input = make_input();
    app_pipe_startup_snapshot_t snapshot;

    app_pipe_startup_init(&startup);
    enter_move_to_startup_position(&startup, &input, &snapshot);
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
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_READY);
    assert(snapshot.id3_allowed);
}

static void test_move_timeout_or_feedback_loss_locks_only_id3(void)
{
    app_pipe_startup_t startup;
    app_pipe_startup_input_t input = make_input();
    app_pipe_startup_snapshot_t snapshot;

    app_pipe_startup_init(&startup);
    enter_move_to_startup_position(&startup, &input, &snapshot);
    input.now_ms = 10001U;
    app_pipe_startup_step(&startup, &input, &snapshot);
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_ID3_LOCKED);
    assert(!snapshot.id3_allowed);
    assert(snapshot.other_motors_allowed);

    app_pipe_startup_init(&startup);
    input = make_input();
    enter_move_to_startup_position(&startup, &input, &snapshot);
    input.id3_feedback_valid = false;
    app_pipe_startup_step(&startup, &input, &snapshot);
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_ID3_LOCKED);
    assert(snapshot.other_motors_allowed);
}

static void test_ready_returns_to_home_gate_when_balance_zero_is_lost(void)
{
    app_pipe_startup_t startup;
    app_pipe_startup_input_t input = make_input();
    app_pipe_startup_snapshot_t snapshot;

    app_pipe_startup_init(&startup);
    reach_ready(&startup, &input, &snapshot);

    input.balance_zero_valid = false;
    app_pipe_startup_step(&startup, &input, &snapshot);
    assert(snapshot.state == APP_PIPE_STARTUP_STATE_WAIT_HOME);
    assert(!snapshot.id3_allowed);
}

int main(void)
{
    test_wait_home_only_disables_id3();
    test_home_enters_move_before_ready();
    test_move_requires_position_and_speed_to_remain_stable();
    test_move_timeout_or_feedback_loss_locks_only_id3();
    test_ready_returns_to_home_gate_when_balance_zero_is_lost();
    return 0;
}
