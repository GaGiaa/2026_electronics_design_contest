#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "drivers/encoder/board_encoder.h"
#include "algorithms/encoder/encoder_quadrature.h"
#include "algorithms/encoder/encoder_speed_filter.h"
#include "algorithms/motor_control/motor_control.h"
#include "pid.h"
#include "protocols/vofa/vofa_justfloat.h"

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
    expect_close(pid.raw_output, 5.0f, 0.001f,
                 "incremental PID must expose the pre-limit total output");
    expect_close(PID_Incremental_Calc(&pid, 10.0f, 0.0f), 5.0f, 0.001f,
                 "unchanged error must not add proportional output twice");
    PID_Incremental_Reset(&pid);
    expect_close(PID_Incremental_Calc(&pid, 10.0f, 0.0f), 5.0f, 0.001f,
                 "incremental PID reset must clear prior error history");
}

static void test_incremental_pid_applies_speed_loop_protections(void)
{
    const PID_Incremental_Param_Config params = {
        .kp = 0.0f,
        .ki = 10.0f,
        .kd = 1.0f,
        .output_limit = 1.0f,
        .deadband = 0.0f,
        .integral_output_limit = 10.0f,
        .integral_separation_threshold = 2.0f,
        .derivative_filter_N = 0.0f,
        .output_delta_limit = 0.2f,
    };
    PID_Incremental pid;

    PID_Incremental_Init(&pid, &params, 0.1f);
    expect_close(PID_Incremental_Calc(&pid, 10.0f, 0.0f), 0.0f, 0.001f,
                 "incremental PID must separate integration outside the error threshold");
    expect_close(pid.raw_output, 0.0f, 0.001f,
                 "pre-limit output must be available when integral separation is active");
    expect_close(PID_Incremental_Calc(&pid, 1.0f, 0.0f), 0.2f, 0.001f,
                 "incremental PID output must obey the per-cycle slew limit");
    expect_close(pid.raw_output, 1.0f, 0.001f,
                 "pre-limit output must precede the output slew limit");
    expect_close(PID_Incremental_Calc(&pid, 1.0f, 0.0f), 0.4f, 0.001f,
                 "incremental PID must accumulate an allowed integral contribution");
    expect_close(PID_Incremental_Calc(&pid, 1.0f, 0.0f), 0.6f, 0.001f,
                 "incremental PID must continue toward its unclamped output");
    expect_close(PID_Incremental_Calc(&pid, 1.0f, 0.0f), 0.8f, 0.001f,
                 "incremental PID must keep the output rate bounded");
    expect_close(PID_Incremental_Calc(&pid, 1.0f, 0.0f), 1.0f, 0.001f,
                 "incremental PID must reach the configured output limit");
    expect_close(PID_Incremental_Calc(&pid, 1.0f, 0.0f), 1.0f, 0.001f,
                 "incremental PID anti-windup must hold at the output limit");
    expect_close(PID_Incremental_Calc(&pid, -1.0f, 0.0f), 0.8f, 0.001f,
                 "incremental PID must release a saturated output when the error reverses");

    PID_Incremental_Reset(&pid);
    expect_close(PID_Incremental_Calc(&pid, 10.0f, 0.0f), 0.0f, 0.001f,
                 "incremental PID reset must clear protection state");
    expect_close(PID_Incremental_Calc(&pid, 20.0f, 0.0f), 0.0f, 0.001f,
                 "feedback-based derivative must ignore a target-only step");
}

static void test_encoder_speed_filter_tracks_fractional_window_average(void)
{
    encoder_speed_filter_t filter;
    int32_t average_counts_q8;

    encoder_speed_filter_init(&filter);
    average_counts_q8 = encoder_speed_filter_update(&filter, 1);
    expect_true(average_counts_q8 == (1 << ENCODER_SPEED_FILTER_FRACTIONAL_BITS),
                "speed filter must report the first count without startup attenuation");
    (void)encoder_speed_filter_update(&filter, 2);
    (void)encoder_speed_filter_update(&filter, 3);
    (void)encoder_speed_filter_update(&filter, 4);
    average_counts_q8 = encoder_speed_filter_update(&filter, 5);
    expect_true(average_counts_q8 == (3 << ENCODER_SPEED_FILTER_FRACTIONAL_BITS),
                "speed filter must average a full five-sample window");
    average_counts_q8 = encoder_speed_filter_update(&filter, 10);
    expect_true(average_counts_q8 == 1228,
                "speed filter must preserve fractional counts in its fixed-point average");
    average_counts_q8 = encoder_speed_filter_update(&filter, -10);
    expect_true(average_counts_q8 == 614,
                "speed filter must handle signed direction changes in the window");
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

static void test_debug_speed_uses_selected_wheel_defaults_without_override(void)
{
    board_encoder_sample_t samples[BOARD_MOTOR_COUNT] = {0};

    motor_control_init();
    g_motor_debug.enable = true;
    g_motor_debug.wheel = BOARD_MOTOR_REAR_RIGHT;
    g_motor_debug.mode = MOTOR_CONTROL_DEBUG_MODE_SPEED;
    g_motor_debug.target_speed_mm_per_s = 100.0f;
    motor_control_step(samples);
    motor_control_step(samples);

    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_REAR_RIGHT), 6.0f, 0.001f,
                 "debug speed must use the selected wheel default PID without explicit override");

    g_motor_debug.wheel = BOARD_MOTOR_FRONT_LEFT;
    motor_control_step(samples);
    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT), 0.0f, 0.001f,
                 "changing the debug wheel must hold output at zero for one control step");
    motor_control_step(samples);

    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_FRONT_LEFT), 23.0f, 0.001f,
                 "debug speed must switch to the new wheel default PID after selection");
}

static void test_debug_speed_explicit_pid_override_is_applied(void)
{
    board_encoder_sample_t samples[BOARD_MOTOR_COUNT] = {0};

    motor_control_init();
    g_motor_debug.enable = true;
    g_motor_debug.wheel = BOARD_MOTOR_REAR_RIGHT;
    g_motor_debug.mode = MOTOR_CONTROL_DEBUG_MODE_SPEED;
    g_motor_debug.use_speed_pid_override = true;
    g_motor_debug.speed_pid_params.kp = 0.9f;
    g_motor_debug.speed_pid_params.ki = 0.0f;
    g_motor_debug.speed_pid_params.kd = 0.0f;
    g_motor_debug.target_speed_mm_per_s = 100.0f;
    motor_control_step(samples);
    motor_control_step(samples);

    expect_close(motor_control_get_output_duty_percent(BOARD_MOTOR_REAR_RIGHT), 90.0f, 0.001f,
                 "debug speed must apply PID values only when explicit override is enabled");
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
    g_motor_debug.use_speed_pid_override = true;
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
    g_motor_debug.use_speed_pid_override = true;
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
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE_3];
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

static void test_vofa_justfloat_encodes_raw_and_filtered_speed_channels(void)
{
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE(VOFA_JUSTFLOAT_CHANNEL_COUNT)];
    static const uint8_t expected[] = {
        0x00U, 0x00U, 0xC0U, 0x3FU,
        0x00U, 0x00U, 0x10U, 0xC0U,
        0x00U, 0x00U, 0x48U, 0x42U,
        0x00U, 0x00U, 0x20U, 0x41U,
        0x00U, 0x00U, 0x80U, 0x7FU,
    };
    size_t index;

    expect_true(vofa_justfloat_encode4(frame, sizeof(frame), 1.5f, -2.25f, 50.0f, 10.0f),
                "JustFloat encoder must accept four speed-control channels");
    for (index = 0U; index < sizeof(expected); ++index) {
        expect_true(frame[index] == expected[index],
                    "JustFloat four-channel frame must preserve channel order and tail");
    }
}

static void test_encoder_configuration_derives_counts_from_mechanics_and_mode(void)
{
    expect_true(BOARD_ENCODER_MOTOR_LINES_PER_REVOLUTION == 13U,
                "encoder motor-line configuration must expose the 13-line encoder");
    expect_true(BOARD_ENCODER_GEAR_RATIO == 20U,
                "encoder mechanics must expose the 20:1 gearbox ratio");
    expect_true(BOARD_ENCODER_OUTPUT_SHAFT_LINES_PER_REVOLUTION == 260U,
                "output shaft lines must be derived from motor lines and gear ratio");
    expect_true(BOARD_ENCODER_A_PHASE_DUAL_EDGE_MULTIPLIER == 2U,
                "A-phase dual-edge decoding must have a two-count multiplier");
    expect_true(BOARD_ENCODER_AB_PHASE_QUADRATURE_X4_MULTIPLIER == 4U,
                "AB quadrature decoding must have a four-count multiplier");
#if BOARD_ENCODER_DECODE_MODE == BOARD_ENCODER_DECODE_MODE_A_PHASE_DUAL_EDGE
    expect_true(BOARD_ENCODER_COUNTS_PER_REVOLUTION == 520U,
                "A-phase dual-edge decoding must derive 520 counts per output revolution");
#else
    expect_true(BOARD_ENCODER_COUNTS_PER_REVOLUTION == 1040U,
                "AB quadrature X4 decoding must derive 1040 counts per output revolution");
#endif
}

static void test_quadrature_decoder_tracks_valid_edges_and_rejects_invalid_transitions(void)
{
    board_encoder_quadrature_t decoder;

    board_encoder_quadrature_init(&decoder, false, false);
    expect_true(board_encoder_quadrature_update(&decoder, true, false) == 1,
                "quadrature decoder must count the first forward transition");
    expect_true(board_encoder_quadrature_update(&decoder, true, true) == 1,
                "quadrature decoder must count each forward transition");
    expect_true(board_encoder_quadrature_update(&decoder, false, true) == 1,
                "quadrature decoder must preserve forward direction across the cycle");
    expect_true(board_encoder_quadrature_update(&decoder, false, false) == 1,
                "quadrature decoder must complete a forward cycle with four counts");

    board_encoder_quadrature_init(&decoder, false, false);
    expect_true(board_encoder_quadrature_update(&decoder, false, true) == -1,
                "quadrature decoder must count reverse transitions negatively");
    expect_true(board_encoder_quadrature_update(&decoder, true, true) == -1,
                "quadrature decoder must preserve reverse direction across the cycle");
    expect_true(board_encoder_quadrature_update(&decoder, true, false) == -1,
                "quadrature decoder must keep reverse sign for every valid edge");
    expect_true(board_encoder_quadrature_update(&decoder, false, false) == -1,
                "quadrature decoder must complete a reverse cycle with four counts");

    board_encoder_quadrature_init(&decoder, false, false);
    expect_true(board_encoder_quadrature_update(&decoder, true, true) == 0,
                "quadrature decoder must ignore illegal two-bit transitions");
    expect_true(board_encoder_quadrature_update(&decoder, true, true) == 0,
                "quadrature decoder must ignore unchanged states");
}

int main(void)
{
    test_incremental_pid_accumulates_and_resets();
    test_incremental_pid_applies_speed_loop_protections();
    test_encoder_speed_filter_tracks_fractional_window_average();
    test_position_pid_applies_deadband_and_limit();
    test_normal_speed_targets_drive_wheels_independently();
    test_debug_pwm_overrides_only_selected_wheel();
    test_debug_mode_change_resets_speed_controller();
    test_debug_speed_uses_selected_wheel_defaults_without_override();
    test_debug_speed_explicit_pid_override_is_applied();
    test_debug_mode_change_holds_output_zero_for_one_step();
    test_invalid_debug_speed_parameters_stop_all_wheels();
    test_zero_speed_target_resets_accumulated_output();
    test_nonfinite_debug_duty_stops_output();
    test_vofa_justfloat_encodes_three_float_channels();
    test_vofa_justfloat_encodes_raw_and_filtered_speed_channels();
    test_encoder_configuration_derives_counts_from_mechanics_and_mode();
    test_quadrature_decoder_tracks_valid_edges_and_rejects_invalid_transitions();
    puts("PASS: motor PID and control tests passed.");
    return EXIT_SUCCESS;
}
