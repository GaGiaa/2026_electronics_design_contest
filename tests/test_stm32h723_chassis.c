#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "app_chassis.h"
#include "app_crsf.h"
#include "app_m2006.h"

static uint8_t crc8(const uint8_t *data, uint32_t length)
{
    uint8_t crc = 0U;
    uint32_t index;
    uint8_t bit;

    for (index = 0U; index < length; ++index) {
        crc ^= data[index];
        for (bit = 0U; bit < 8U; ++bit) {
            crc = (crc & 0x80U) != 0U ? (uint8_t)((crc << 1U) ^ 0xD5U) :
                                        (uint8_t)(crc << 1U);
        }
    }
    return crc;
}

static void make_channels_frame(uint8_t frame[26], const uint16_t channels[16])
{
    uint32_t channel;

    memset(frame, 0, 26U);
    frame[0] = 0xC8U;
    frame[1] = 24U;
    frame[2] = 0x16U;
    for (channel = 0U; channel < 16U; ++channel) {
        uint32_t bit_offset = channel * 11U;
        uint32_t byte_offset = bit_offset / 8U;
        uint32_t packed = (uint32_t)channels[channel] << (bit_offset % 8U);

        frame[3U + byte_offset] |= (uint8_t)packed;
        frame[3U + byte_offset + 1U] |= (uint8_t)(packed >> 8U);
        frame[3U + byte_offset + 2U] |= (uint8_t)(packed >> 16U);
    }
    frame[25] = crc8(&frame[2], 23U);
}

static void test_crsf_manual_mix_and_switch_guard(void)
{
    app_crsf_parser_t parser;
    app_crsf_input_t input = {0};
    uint16_t channels[16] = {992U};
    uint8_t frame[26];
    uint32_t index;
    app_chassis_command_t command;

    channels[2] = 1811U;
    channels[0] = 992U;
    channels[4] = 992U;
    channels[6] = 992U;
    channels[7] = 172U;
    make_channels_frame(frame, channels);
    app_crsf_parser_init(&parser);
    for (index = 0U; index < sizeof(frame); ++index) {
        (void)app_crsf_parser_feed(&parser, frame[index], 10U, &input);
    }
    assert(input.valid);
    assert(input.channels[2] == 1811U);

    app_chassis_mix(&input, 10U, &command);
    assert(!command.manual_active);
    assert(command.mode == APP_CHASSIS_MODE_REMOTE_IDLE);
    assert(command.left_target_rpm == 0.0f);
    assert(command.right_target_rpm == 0.0f);
    assert(command.third_motor_target_rpm == 0.0f);

    input.channels[4] = 1811U;
    app_chassis_mix(&input, 10U, &command);
    assert(command.manual_active);
    assert(command.mode == APP_CHASSIS_MODE_MANUAL);
    assert(command.left_target_rpm == 550.0f);
    assert(command.right_target_rpm == -550.0f);

    input.channels[6] = 172U;
    app_chassis_mix(&input, 10U, &command);
    assert(!command.manual_active);
    assert(command.mode == APP_CHASSIS_MODE_STOP);
    assert(command.left_target_rpm == 0.0f);

    input.channels[6] = 992U;
    input.channels[7] = 992U;
    input.channels[2] = 992U;
    app_chassis_mix(&input, 10U, &command);
    assert(command.manual_active);
    assert(command.mode == APP_CHASSIS_MODE_LINE_FOLLOW);
    assert(fabsf(command.base_speed_mm_s - APP_H723_LINE_FOLLOW_BASE_SPEED_MM_S) < 0.0001f);
    assert(fabsf(command.left_target_rpm -
                 APP_H723_LINE_FOLLOW_BASE_SPEED_MM_S * APP_H723_MM_S_TO_OUTPUT_RPM) < 0.0001f);
    assert(fabsf(command.right_target_rpm +
                 APP_H723_LINE_FOLLOW_BASE_SPEED_MM_S * APP_H723_MM_S_TO_OUTPUT_RPM) < 0.0001f);

    input.channels[6] = 1811U;
    input.channels[7] = 992U;
    app_chassis_mix(&input, 10U, &command);
    assert(!command.manual_active);
    assert(command.mode == APP_CHASSIS_MODE_STOP);

    input.channels[6] = 992U;
    input.channels[7] = 172U;
    input.channels[2] = 1811U;
    input.channels[0] = 1811U;
    app_chassis_mix(&input, 10U, &command);
    assert(command.manual_active);
    assert(command.left_target_rpm == 550.0f);
    assert(command.right_target_rpm == 0.0f);

    app_chassis_mix(&input, 111U, &command);
    assert(!command.manual_active);
    assert(command.left_target_rpm == 0.0f);
    assert(command.right_target_rpm == 0.0f);
}

static void test_crsf_crc_rejection(void)
{
    app_crsf_parser_t parser;
    app_crsf_input_t input = {0};
    uint16_t channels[16] = {992U};
    uint8_t frame[26];
    uint32_t index;

    make_channels_frame(frame, channels);
    frame[25] ^= 0x01U;
    app_crsf_parser_init(&parser);
    for (index = 0U; index < sizeof(frame); ++index) {
        (void)app_crsf_parser_feed(&parser, frame[index], 10U, &input);
    }
    assert(!input.valid);
    assert(input.crc_error_count == 1U);
}

static void test_m2006_feedback_and_group_command(void)
{
    const uint8_t feedback[8] = {0x10U, 0x00U, 0x01U, 0xF4U, 0xFFU, 0x38U, 0x55U, 0U};
    app_m2006_feedback_t parsed;
    uint8_t command[8];

    assert(app_m2006_parse_feedback(0x201U, feedback, &parsed));
    assert(parsed.motor_id == 1U);
    assert(parsed.encoder == 4096U);
    assert(parsed.rotor_speed_rpm == 500);
    assert(parsed.output_speed_rpm == (500.0f / 36.0f));
    assert(parsed.current_raw == -200);
    assert(parsed.current_a == (-200.0f * 10.0f / 16384.0f));
    assert(app_m2006_parse_feedback(0x203U, feedback, &parsed));
    assert(parsed.motor_id == 3U);
    app_m2006_encode_group_current(1000, -1000, command);
    assert(command[0] == 0x03U && command[1] == 0xE8U);
    assert(command[2] == 0xFCU && command[3] == 0x18U);
    assert(command[4] == 0U && command[7] == 0U);
    {
        const int16_t currents[3] = {100, -200, 300};
        app_m2006_encode_group_current_slots(currents, command);
        assert(command[0] == 0U && command[1] == 100U);
        assert(command[2] == 0xFFU && command[3] == 0x38U);
        assert(command[4] == 1U && command[5] == 44U);
        assert(command[6] == 0U && command[7] == 0U);
    }
}

static void test_m2006_current_conversion(void)
{
    assert(app_m2006_raw_current_to_a(16384) == 10.0f);
    assert(app_m2006_raw_current_to_a(-16384) == -10.0f);
    assert(app_m2006_current_a_to_raw(2.0f) == 3277);
    assert(app_m2006_current_a_to_raw(-2.0f) == -3277);
    assert(app_m2006_current_a_to_raw(100.0f) == 16384);
    assert(app_m2006_current_a_to_raw(-100.0f) == -16384);
}

static void test_m2006_position_tracker_unwraps_and_converts_output_angle(void)
{
    app_m2006_position_tracker_t tracker;

    app_m2006_position_tracker_init(&tracker);
    assert(!app_m2006_position_tracker_update(&tracker, 8190U));
    assert(app_m2006_position_tracker_update(&tracker, 2U));
    assert(app_m2006_position_tracker_motor_counts(&tracker) == 4);
    assert(app_m2006_position_tracker_update(&tracker, 8190U));
    assert(app_m2006_position_tracker_motor_counts(&tracker) == 0);

    app_m2006_position_tracker_init(&tracker);
    assert(!app_m2006_position_tracker_update(&tracker, 0U));
    assert(app_m2006_position_tracker_update(&tracker, 0U));
    tracker.motor_counts = 8192LL * 36LL;
    assert(app_m2006_position_tracker_output_degrees(&tracker) == 360.0f);
}

int main(void)
{
    test_crsf_manual_mix_and_switch_guard();
    test_crsf_crc_rejection();
    test_m2006_feedback_and_group_command();
    test_m2006_current_conversion();
    test_m2006_position_tracker_unwraps_and_converts_output_angle();
    return 0;
}
