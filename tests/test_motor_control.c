#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "motor_control.h"
#include "pid.h"
#include "vofa_justfloat.h"

static void expect_close(float actual, float expected, float tolerance, const char *message)
{
    if (fabsf(actual - expected) > tolerance) {
        fprintf(stderr, "%s: expected %.4f, got %.4f\n", message, expected, actual);
        exit(EXIT_FAILURE);
    }
}

static void expect_true(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void test_incremental_pid_accumulates_and_resets(void)
{
    const PID_Incremental_Param_Config params = {
        .kp = 0.5f,
        .ki = 0.0f,
        .kd = 0.0f,
        .output_limit = 100.0f,
        .deadband = 0.0f,
    };
    PID_Incremental pid;

    PID_Incremental_Init(&pid, &params, 0.01f);
    expect_close(PID_Incremental_Calc(&pid, 10.0f, 0.0f), 5.0f, 0.001f,
                 "incremental PID must apply the proportional increment");
    expect_close(PID_Incremental_Calc(&pid, 10.0f, 0.0f), 5.0f, 0.001f,
                 "unchanged error must not add proportional output twice");
    PID_Incremental_Reset(&pid);
    expect_close(PID_Incremental_Calc(&pid, 10.0f, 0.0f), 5.0f, 0.001f,
                 "incremental PID reset must clear prior error history");
}

static void test_position_pid_applies_deadband_and_limit(void)
{
    const PID_Position_Param_Config params = {
        .kp = 2.0f,
        .ki = 0.0f,
        .kd = 0.0f,
        .output_limit = 3.0f,
        .deadband = 1.0f,
    };
    PID_Position pid;

    PID_Position_Init(&pid, &params, 0.01f);
    expect_close(PID_Position_Calc(&pid, 0.5f, 0.0f), 0.0f, 0.001f,
                 "position PID must suppress errors inside the deadband");
    expect_close(PID_Position_Calc(&pid, 10.0f, 0.0f), 3.0f, 0.001f,
                 "position PID must clamp output to its configured limit");
}

static void test_normal_speed_targets_drive_wheels_independently(void)
{
    board_encoder_sample_t samples[BOARD_MOTOR_COUNT] = {0};

    motor_control_init();
    g_motor_speed_targets_mm_s[BOARD_MOTOR_FRONT_LEFT] = 100.0f;
    motor_control_step(samples);

    expect_true(motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT) > 0.0f,
                "front-left speed target must produce a forward duty command");
    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_RIGHT), 0.0f, 0.001f,
                 "uncommanded front-right wheel must remain stopped");
}

static void test_debug_pwm_overrides_only_selected_wheel(void)
{
    board_encoder_sample_t samples[BOARD_MOTOR_COUNT] = {0};

    motor_control_init();
    g_motor_speed_targets_mm_s[BOARD_MOTOR_FRONT_LEFT] = 100.0f;
    g_motor_debug.enable = true;
    g_motor_debug.wheel = BOARD_MOTOR_REAR_RIGHT;
    g_motor_debug.mode = MOTOR_CONTROL_DEBUG_MODE_PWM;
    g_motor_debug.target_duty_percent = -25.0f;
    motor_control_step(samples);
    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_REAR_RIGHT), 0.0f, 0.001f,
                 "enabling debug output must hold the selected wheel at zero for one control step");
    motor_control_step(samples);

    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_REAR_RIGHT), -25.0f, 0.001f,
                 "debug PWM must apply the requested signed duty to its selected wheel");
    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT), 0.0f, 0.001f,
                 "debug PWM must stop every non-selected wheel");
}

static void test_debug_mode_change_resets_speed_controller(void)
{
    board_encoder_sample_t samples[BOARD_MOTOR_COUNT] = {0};

    motor_control_init();
    g_motor_debug.enable = true;
    g_motor_debug.wheel = BOARD_MOTOR_FRONT_LEFT;
    g_motor_debug.mode = MOTOR_CONTROL_DEBUG_MODE_SPEED;
    g_motor_debug.target_speed_mm_per_s = 100.0f;
    motor_control_step(samples);
    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT), 0.0f, 0.001f,
                 "entering debug speed mode must hold output at zero for one control step");
    motor_control_step(samples);
    expect_true(motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT) > 0.0f,
                "debug speed mode must run the selected speed controller");

    g_motor_debug.mode = MOTOR_CONTROL_DEBUG_MODE_STOP;
    motor_control_step(samples);
    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT), 0.0f, 0.001f,
                 "debug stop mode must reset the selected controller and stop output");
}

static void test_debug_mode_change_holds_output_zero_for_one_step(void)
{
    board_encoder_sample_t samples[BOARD_MOTOR_COUNT] = {0};

    motor_control_init();
    g_motor_debug.enable = true;
    g_motor_debug.wheel = BOARD_MOTOR_FRONT_LEFT;
    g_motor_debug.mode = MOTOR_CONTROL_DEBUG_MODE_PWM;
    g_motor_debug.target_duty_percent = 30.0f;
    motor_control_step(samples);
    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT), 0.0f, 0.001f,
                 "entering debug PWM mode must hold output at zero for one control step");
    motor_control_step(samples);
    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT), 30.0f, 0.001f,
                 "initial debug PWM command must reach the selected wheel");

    g_motor_debug.mode = MOTOR_CONTROL_DEBUG_MODE_SPEED;
    g_motor_debug.target_speed_mm_per_s = 100.0f;
    motor_control_step(samples);
    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT), 0.0f, 0.001f,
                 "debug mode changes must hold output at zero for one control step");
}

static void test_invalid_debug_speed_parameters_stop_all_wheels(void)
{
    board_encoder_sample_t samples[BOARD_MOTOR_COUNT] = {0};

    motor_control_init();
    g_motor_speed_targets_mm_s[BOARD_MOTOR_FRONT_LEFT] = 100.0f;
    g_motor_debug.enable = true;
    g_motor_debug.wheel = BOARD_MOTOR_FRONT_RIGHT;
    g_motor_debug.mode = MOTOR_CONTROL_DEBUG_MODE_SPEED;
    g_motor_debug.speed_pid_params.output_limit = 0.0f;
    motor_control_step(samples);
    motor_control_step(samples);

    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT), 0.0f, 0.001f,
                 "invalid debug PID parameters must stop normal four-wheel output");
    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_RIGHT), 0.0f, 0.001f,
                 "invalid debug PID parameters must stop the selected wheel");
}

static void test_zero_speed_target_resets_accumulated_output(void)
{
    board_encoder_sample_t samples[BOARD_MOTOR_COUNT] = {0};

    motor_control_init();
    g_motor_debug.enable = true;
    g_motor_debug.wheel = BOARD_MOTOR_FRONT_LEFT;
    g_motor_debug.mode = MOTOR_CONTROL_DEBUG_MODE_SPEED;
    g_motor_debug.target_speed_mm_per_s = 100.0f;
    g_motor_debug.speed_pid_params.ki = 1.0f;
    motor_control_step(samples);
    motor_control_step(samples);
    expect_true(motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT) > 0.0f,
                "speed PID must accumulate a nonzero command before the stop request");

    g_motor_debug.target_speed_mm_per_s = 0.0f;
    motor_control_step(samples);
    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT), 0.0f, 0.001f,
                 "zero speed target must clear accumulated PWM output");
}

static void test_nonfinite_debug_duty_stops_output(void)
{
    board_encoder_sample_t samples[BOARD_MOTOR_COUNT] = {0};

    motor_control_init();
    g_motor_debug.enable = true;
    g_motor_debug.wheel = BOARD_MOTOR_FRONT_LEFT;
    g_motor_debug.mode = MOTOR_CONTROL_DEBUG_MODE_PWM;
    g_motor_debug.target_duty_percent = NAN;
    motor_control_step(samples);
    motor_control_step(samples);

    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT), 0.0f, 0.001f,
                 "nonfinite debug PWM must be rejected as a stop command");
}

static void test_vofa_justfloat_encodes_three_float_channels(void)
{
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE];
    static const uint8_t expected[] = {
        0x00U, 0x00U, 0xC0U, 0x3FU,
        0x00U, 0x00U, 0x10U, 0xC0U,
        0x00U, 0x00U, 0x48U, 0x42U,
        0x00U, 0x00U, 0x80U, 0x7FU,
    };
    size_t index;

    expect_true(vofa_justfloat_encode3(frame, sizeof(frame), 1.5f, -2.25f, 50.0f),
                "JustFloat encoder must accept a correctly sized frame buffer");
    for (index = 0U; index < sizeof(expected); ++index) {
        expect_true(frame[index] == expected[index],
                    "JustFloat frame must use little-endian float32 channels and the VOFA tail");
    }
    expect_true(!vofa_justfloat_encode3(frame, sizeof(frame) - 1U, 0.0f, 0.0f, 0.0f),
                "JustFloat encoder must reject a frame buffer with the wrong fixed length");
}

int main(void)
{
    test_incremental_pid_accumulates_and_resets();
    test_position_pid_applies_deadband_and_limit();
    test_normal_speed_targets_drive_wheels_independently();
    test_debug_pwm_overrides_only_selected_wheel();
    test_debug_mode_change_resets_speed_controller();
    test_debug_mode_change_holds_output_zero_for_one_step();
    test_invalid_debug_speed_parameters_stop_all_wheels();
    test_zero_speed_target_resets_accumulated_output();
    test_nonfinite_debug_duty_stops_output();
    test_vofa_justfloat_encodes_three_float_channels();
    puts("PASS: motor PID and control tests passed.");
    return EXIT_SUCCESS;
}
