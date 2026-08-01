#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "app_chassis.h"
#include "app_config.h"

static app_crsf_input_t make_input(void)
{
    app_crsf_input_t input = {0};
    uint32_t index;

    for (index = 0U; index < APP_CRSF_CHANNEL_COUNT; ++index) {
        input.channels[index] = 992U;
    }
    input.valid = true;
    input.last_valid_ms = 0U;
    return input;
}

static void step(app_chassis_control_state_t *state,
                 app_crsf_input_t *input,
                 uint32_t button_mask,
                 uint32_t now_ms,
                 app_chassis_control_output_t *output)
{
    app_chassis_control_step(state, input, button_mask, now_ms, output);
}

static void test_menu_uses_button_mapping_and_confirm_request(void)
{
    app_chassis_control_state_t state;
    app_chassis_control_output_t output;
    app_crsf_input_t input = make_input();

    app_chassis_control_init(&state);
    step(&state, &input, 0U, 0U, &output);
    assert(output.mode == APP_CHASSIS_MODE_TASK_MENU);
    assert(!output.remote_takeover);
    assert(output.buttons_enabled);
    assert(output.selected_task == 2U);

    step(&state, &input, 1U << 1U, 1U, &output);
    assert(output.selected_task == 6U);
    step(&state, &input, 0U, 121U, &output);
    step(&state, &input, 1U << 2U, 122U, &output);
    assert(output.selected_task == 2U);
    step(&state, &input, 0U, 123U, &output);
    step(&state, &input, 1U << 0U, 244U, &output);
    assert(output.task_request_available);
    assert(output.selected_task == 2U);
}

static void test_se_takes_over_and_masks_buttons_until_release(void)
{
    app_chassis_control_state_t state;
    app_chassis_control_output_t output;
    app_crsf_input_t input = make_input();

    app_chassis_control_init(&state);
    step(&state, &input, 0U, 0U, &output);
    input.channels[APP_H723_CRSF_SE_CHANNEL_INDEX] = 1811U;
    input.channels[APP_H723_CRSF_SB_CHANNEL_INDEX] = 172U;
    step(&state, &input, 1U << 1U, 1U, &output);
    assert(output.remote_takeover);
    assert(!output.buttons_enabled);
    assert(output.mode == APP_CHASSIS_MODE_REMOTE_IDLE);
    assert(output.selected_task == 2U);

    input.channels[APP_H723_CRSF_SE_CHANNEL_INDEX] = 992U;
    step(&state, &input, 1U << 1U, 2U, &output);
    assert(output.mode == APP_CHASSIS_MODE_TASK_MENU);
    assert(output.buttons_enabled);
    assert(output.selected_task == 2U);
    assert(!output.task_request_available);
}

static void test_remote_modes_and_timeout_are_safe(void)
{
    app_chassis_control_state_t state;
    app_chassis_control_output_t output;
    app_crsf_input_t input = make_input();

    app_chassis_control_init(&state);
    input.channels[APP_H723_CRSF_SE_CHANNEL_INDEX] = 1811U;
    input.channels[APP_H723_CRSF_SB_CHANNEL_INDEX] = 992U;
    input.channels[APP_H723_CRSF_SC_CHANNEL_INDEX] = 172U;
    input.channels[2] = 1811U;
    input.channels[0] = 992U;
    step(&state, &input, 0U, 0U, &output);
    step(&state, &input, 0U, 1U, &output);
    assert(output.mode == APP_CHASSIS_MODE_REMOTE_MANUAL);
    assert(output.chassis.manual_active);
    assert(output.chassis.left_target_rpm == APP_H723_CHASSIS_MAX_OUTPUT_RPM);
    assert(output.chassis.right_target_rpm == -APP_H723_CHASSIS_MAX_OUTPUT_RPM);
    assert(output.chassis.third_motor_target_rpm == 0.0f);

    input.channels[APP_H723_CRSF_SC_CHANNEL_INDEX] = 992U;
    step(&state, &input, 0U, 2U, &output);
    assert(output.mode == APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW);
    assert(output.chassis.third_motor_target_rpm == 0.0f);

    input.channels[APP_H723_CRSF_SC_CHANNEL_INDEX] = 1811U;
    step(&state, &input, 0U, 3U, &output);
    assert(output.mode == APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW_BALL);
    assert(output.chassis.manual_active);
    assert(fabsf(output.chassis.base_speed_mm_s -
                 APP_H723_LINE_FOLLOW_STICK_RANGE_MM_S) < 0.0001f);

    input.channels[APP_H723_CRSF_SB_CHANNEL_INDEX] = 172U;
    step(&state, &input, 0U, 4U, &output);
    assert(output.mode == APP_CHASSIS_MODE_REMOTE_IDLE);
    assert(output.chassis.left_target_rpm == 0.0f);
    assert(output.chassis.right_target_rpm == 0.0f);

    input.channels[APP_H723_CRSF_SB_CHANNEL_INDEX] = 992U;
    input.channels[APP_H723_CRSF_SC_CHANNEL_INDEX] = 172U;
    step(&state, &input, 0U, APP_H723_CRSF_TIMEOUT_MS + 1U, &output);
    assert(output.mode == APP_CHASSIS_MODE_REMOTE_IDLE);
    assert(output.chassis.left_target_rpm == 0.0f);
    assert(output.chassis.right_target_rpm == 0.0f);
}

int main(void)
{
    test_menu_uses_button_mapping_and_confirm_request();
    test_se_takes_over_and_masks_buttons_until_release();
    test_remote_modes_and_timeout_are_safe();
    return 0;
}
