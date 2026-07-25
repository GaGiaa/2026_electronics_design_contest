#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "board_motor.h"
#include "crsf_control.h"
#include "crsf_protocol.h"

static uint8_t crc8_dvb_s2(const uint8_t *data, size_t length)
{
    uint8_t crc = 0U;
    size_t index;

    for (index = 0U; index < length; ++index) {
        uint8_t bit;

        crc ^= data[index];
        for (bit = 0U; bit < 8U; ++bit) {
            crc = (crc & 0x80U) != 0U ? (uint8_t)((crc << 1U) ^ 0xD5U)
                                      : (uint8_t)(crc << 1U);
        }
    }
    return crc;
}

static size_t encode_channels(const uint16_t channels[CRSF_CHANNEL_COUNT],
                              uint8_t frame[CRSF_MAX_FRAME_SIZE])
{
    uint8_t payload[CRSF_CHANNEL_PAYLOAD_SIZE] = {0U};
    size_t bit_offset = 0U;
    size_t channel;

    for (channel = 0U; channel < CRSF_CHANNEL_COUNT; ++channel) {
        uint16_t value = channels[channel] & 0x07FFU;
        size_t bit;

        for (bit = 0U; bit < 11U; ++bit) {
            if ((value & (1U << bit)) != 0U) {
                payload[bit_offset / 8U] |= (uint8_t)(1U << (bit_offset % 8U));
            }
            ++bit_offset;
        }
    }

    frame[0U] = CRSF_ADDRESS_RECEIVER;
    frame[1U] = CRSF_RC_CHANNELS_FRAME_LENGTH;
    frame[2U] = CRSF_FRAME_TYPE_RC_CHANNELS_PACKED;
    memcpy(&frame[3U], payload, sizeof(payload));
    frame[3U + sizeof(payload)] = crc8_dvb_s2(&frame[2U], 1U + sizeof(payload));
    return 4U + sizeof(payload);
}

static void test_protocol_decodes_all_channels(void)
{
    const uint16_t expected[CRSF_CHANNEL_COUNT] = {
        172U, 992U, 1811U, 1200U, 300U, 700U, 1400U, 1800U,
        512U, 1024U, 1536U, 2047U, 1U, 800U, 1600U, 1000U
    };
    uint8_t frame[CRSF_MAX_FRAME_SIZE] = {0U};
    crsf_parser_t parser;
    crsf_channels_t actual = {0};
    size_t length = encode_channels(expected, frame);
    size_t index;

    crsf_parser_init(&parser);
    for (index = 0U; index < length; ++index) {
        assert(!crsf_parser_feed(&parser, frame[index], &actual) || index == length - 1U);
    }
    for (index = 0U; index < CRSF_CHANNEL_COUNT; ++index) {
        assert(actual.channels[index] == expected[index]);
    }
}

static void test_protocol_accepts_broadcast_address(void)
{
    const uint16_t expected[CRSF_CHANNEL_COUNT] = {992U};
    uint8_t frame[CRSF_MAX_FRAME_SIZE] = {0U};
    crsf_parser_t parser;
    crsf_channels_t actual = {0};
    size_t length = encode_channels(expected, frame);
    size_t index;

    frame[0U] = 0x00U;
    crsf_parser_init(&parser);
    for (index = 0U; index < length; ++index) {
        assert(!crsf_parser_feed(&parser, frame[index], &actual) || index == length - 1U);
    }
    assert(actual.channels[0U] == expected[0U]);
}

static void test_protocol_handles_partial_and_concatenated_frames(void)
{
    const uint16_t first[CRSF_CHANNEL_COUNT] = {992U};
    const uint16_t second[CRSF_CHANNEL_COUNT] = {1811U};
    uint8_t frames[CRSF_MAX_FRAME_SIZE * 2U] = {0U};
    crsf_parser_t parser;
    crsf_channels_t actual = {0};
    size_t first_length = encode_channels(first, frames);
    size_t second_length = encode_channels(second, &frames[first_length]);
    size_t index;
    size_t total_length = first_length + second_length;

    crsf_parser_init(&parser);
    for (index = 0U; index < total_length; ++index) {
        (void)crsf_parser_feed(&parser, frames[index], &actual);
    }
    assert(actual.channels[0U] == 1811U);
}

static void test_protocol_rejects_bad_crc_and_resynchronizes(void)
{
    const uint16_t expected[CRSF_CHANNEL_COUNT] = {992U};
    uint8_t frame[CRSF_MAX_FRAME_SIZE] = {0U};
    crsf_parser_t parser;
    crsf_channels_t actual = {0};
    size_t length = encode_channels(expected, frame);
    size_t index;

    frame[length - 1U] ^= 0x01U;
    crsf_parser_init(&parser);
    for (index = 0U; index < length; ++index) {
        assert(!crsf_parser_feed(&parser, frame[index], &actual));
    }
    assert(actual.channels[0U] == 0U);

    length = encode_channels(expected, frame);
    for (index = 0U; index < length; ++index) {
        (void)crsf_parser_feed(&parser, frame[index], &actual);
    }
    assert(actual.channels[0U] == 992U);
}

static void test_protocol_rejects_bad_length_and_unknown_type(void)
{
    const uint16_t expected[CRSF_CHANNEL_COUNT] = {992U};
    uint8_t frame[CRSF_MAX_FRAME_SIZE] = {0U};
    crsf_parser_t parser;
    crsf_channels_t actual = {0};
    size_t length;
    size_t index;

    crsf_parser_init(&parser);
    (void)crsf_parser_feed(&parser, CRSF_ADDRESS_RECEIVER, &actual);
    (void)crsf_parser_feed(&parser, 1U, &actual);
    assert(parser.frame_error_count == 1U);

    length = encode_channels(expected, frame);
    frame[2U] = 0x14U;
    frame[length - 1U] = crc8_dvb_s2(&frame[2U], frame[1U] - 1U);
    for (index = 0U; index < length; ++index) {
        (void)crsf_parser_feed(&parser, frame[index], &actual);
    }
    assert(actual.channels[0U] == 0U);
    assert(parser.crc_error_count == 0U);
}

static void test_control_applies_deadband_and_diff_drive_mix(void)
{
    crsf_control_input_t input = {
        .valid = true,
        .channels = {992U},
        .last_valid_time_ms = 1000U,
    };
    float targets[BOARD_MOTOR_COUNT] = {0.0f};

    input.channels[CRSF_FORWARD_CHANNEL_INDEX] = 1811U;
    input.channels[CRSF_TURN_CHANNEL_INDEX] = 992U;
    assert(crsf_control_mix(&input, 1050U, targets));
    assert(fabsf(targets[BOARD_MOTOR_FRONT_LEFT] - CRSF_MAX_SPEED_MM_PER_S) < 0.01f);
    assert(fabsf(targets[BOARD_MOTOR_REAR_LEFT] - CRSF_MAX_SPEED_MM_PER_S) < 0.01f);
    assert(fabsf(targets[BOARD_MOTOR_FRONT_RIGHT] - CRSF_MAX_SPEED_MM_PER_S) < 0.01f);
    assert(fabsf(targets[BOARD_MOTOR_REAR_RIGHT] - CRSF_MAX_SPEED_MM_PER_S) < 0.01f);

    input.channels[CRSF_FORWARD_CHANNEL_INDEX] = 1020U;
    assert(crsf_control_mix(&input, 1050U, targets));
    assert(fabsf(targets[BOARD_MOTOR_FRONT_LEFT]) < 0.01f);
    assert(fabsf(targets[BOARD_MOTOR_FRONT_RIGHT]) < 0.01f);
}

static void test_control_uses_observed_radio_mapping(void)
{
    crsf_control_input_t input = {
        .valid = true,
        .channels = {992U},
        .last_valid_time_ms = 1000U,
    };
    float targets[BOARD_MOTOR_COUNT] = {0.0f};

    input.channels[2U] = 1811U;
    input.channels[0U] = 992U;
    assert(crsf_control_mix(&input, 1050U, targets));
    assert(fabsf(targets[BOARD_MOTOR_FRONT_LEFT] - CRSF_MAX_SPEED_MM_PER_S) < 0.01f);
    assert(fabsf(targets[BOARD_MOTOR_FRONT_RIGHT] - CRSF_MAX_SPEED_MM_PER_S) < 0.01f);
}

static void test_control_turns_and_normalizes_combined_command(void)
{
    crsf_control_input_t input = {
        .valid = true,
        .channels = {992U},
        .last_valid_time_ms = 1000U,
    };
    float targets[BOARD_MOTOR_COUNT] = {0.0f};

    input.channels[CRSF_FORWARD_CHANNEL_INDEX] = 1811U;
    input.channels[CRSF_TURN_CHANNEL_INDEX] = 1811U;
    assert(crsf_control_mix(&input, 1050U, targets));
    assert(fabsf(targets[BOARD_MOTOR_FRONT_LEFT] - CRSF_MAX_SPEED_MM_PER_S) < 0.01f);
    assert(fabsf(targets[BOARD_MOTOR_FRONT_RIGHT]) < 0.01f);
    assert(fabsf(targets[BOARD_MOTOR_REAR_LEFT] - CRSF_MAX_SPEED_MM_PER_S) < 0.01f);
    assert(fabsf(targets[BOARD_MOTOR_REAR_RIGHT]) < 0.01f);
}

static void test_control_stops_on_invalid_or_expired_input(void)
{
    crsf_control_input_t input = {
        .valid = true,
        .channels = {992U},
        .last_valid_time_ms = 1000U,
    };
    float targets[BOARD_MOTOR_COUNT] = {0.0f};
    size_t index;

    input.channels[CRSF_FORWARD_CHANNEL_INDEX] = 1811U;
    assert(!crsf_control_mix(&input, 1100U, targets));
    for (index = 0U; index < BOARD_MOTOR_COUNT; ++index) {
        assert(fabsf(targets[index]) < 0.01f);
    }
    input.valid = false;
    assert(!crsf_control_mix(&input, 1000U, targets));
}

static void test_control_classifies_three_position_sb_mode(void)
{
    crsf_control_input_t input = {
        .valid = true,
        .channels = {992U},
        .last_valid_time_ms = 1000U,
    };

    input.channels[CRSF_MODE_CHANNEL_INDEX] = 500U;
    assert(crsf_control_get_drive_mode(&input, 1050U) == CRSF_DRIVE_MODE_IDLE);

    input.channels[CRSF_MODE_CHANNEL_INDEX] = 992U;
    assert(crsf_control_get_drive_mode(&input, 1050U) == CRSF_DRIVE_MODE_MANUAL);

    input.channels[CRSF_MODE_CHANNEL_INDEX] = 1500U;
    assert(crsf_control_get_drive_mode(&input, 1050U) == CRSF_DRIVE_MODE_LINE_TRACKING);

    assert(crsf_control_get_drive_mode(&input, 1101U) == CRSF_DRIVE_MODE_IDLE);
}

int main(void)
{
    test_protocol_decodes_all_channels();
    test_protocol_accepts_broadcast_address();
    test_protocol_handles_partial_and_concatenated_frames();
    test_protocol_rejects_bad_crc_and_resynchronizes();
    test_protocol_rejects_bad_length_and_unknown_type();
    test_control_applies_deadband_and_diff_drive_mix();
    test_control_uses_observed_radio_mapping();
    test_control_turns_and_normalizes_combined_command();
    test_control_stops_on_invalid_or_expired_input();
    test_control_classifies_three_position_sb_mode();
    return 0;
}
