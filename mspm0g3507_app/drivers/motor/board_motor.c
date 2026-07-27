#include "board_motor.h"

#include "ti_msp_dl_config.h"

#define BOARD_MOTOR_PWM_PERIOD_TICKS 4000U

static void write_motor_pair(GPTIMER_Regs *timer, DL_TIMER_CC_INDEX in1_index,
                             DL_TIMER_CC_INDEX in2_index,
                             board_motor_direction_t direction, uint32_t duty)
{
    uint32_t in1_duty = 0U;
    uint32_t in2_duty = 0U;

    if (direction == BOARD_MOTOR_DIRECTION_FORWARD) {
        in1_duty = duty;
    } else if (direction == BOARD_MOTOR_DIRECTION_REVERSE) {
        in2_duty = duty;
    }

    DL_Timer_setCaptureCompareValue(timer, in1_duty, in1_index);
    DL_Timer_setCaptureCompareValue(timer, in2_duty, in2_index);
}

static int32_t get_direction_sign(board_motor_wheel_t wheel)
{
    switch (wheel) {
    case BOARD_MOTOR_FRONT_LEFT:
        return BOARD_MOTOR_FRONT_LEFT_DIRECTION_SIGN;
    case BOARD_MOTOR_FRONT_RIGHT:
        return BOARD_MOTOR_FRONT_RIGHT_DIRECTION_SIGN;
    case BOARD_MOTOR_REAR_LEFT:
        return BOARD_MOTOR_REAR_LEFT_DIRECTION_SIGN;
    case BOARD_MOTOR_REAR_RIGHT:
        return BOARD_MOTOR_REAR_RIGHT_DIRECTION_SIGN;
    default:
        return 1;
    }
}

void board_motor_set(board_motor_wheel_t wheel, board_motor_direction_t direction,
                     uint8_t duty_percent)
{
    uint32_t duty;
    int32_t direction_sign;

    if (duty_percent > 100U) {
        duty_percent = 100U;
    }
    duty = (BOARD_MOTOR_PWM_PERIOD_TICKS * duty_percent) / 100U;
    direction_sign = get_direction_sign(wheel);
    if (direction_sign < 0 && direction == BOARD_MOTOR_DIRECTION_FORWARD) {
        direction = BOARD_MOTOR_DIRECTION_REVERSE;
    } else if (direction_sign < 0 && direction == BOARD_MOTOR_DIRECTION_REVERSE) {
        direction = BOARD_MOTOR_DIRECTION_FORWARD;
    }

    switch (wheel) {
    case BOARD_MOTOR_FRONT_LEFT:
        write_motor_pair(MOTOR_FRONT_LEFT_INST, GPIO_MOTOR_FRONT_LEFT_C0_IDX,
                         GPIO_MOTOR_FRONT_LEFT_C1_IDX, direction, duty);
        break;
    case BOARD_MOTOR_FRONT_RIGHT:
        write_motor_pair(MOTOR_FRONT_RIGHT_INST, GPIO_MOTOR_FRONT_RIGHT_C0_IDX,
                         GPIO_MOTOR_FRONT_RIGHT_C1_IDX, direction, duty);
        break;
    case BOARD_MOTOR_REAR_LEFT:
        write_motor_pair(MOTOR_REAR_LEFT_INST, GPIO_MOTOR_REAR_LEFT_C0_IDX,
                         GPIO_MOTOR_REAR_LEFT_C1_IDX, direction, duty);
        break;
    case BOARD_MOTOR_REAR_RIGHT:
        write_motor_pair(MOTOR_REAR_RIGHT_INST, GPIO_MOTOR_REAR_RIGHT_C0_IDX,
                         GPIO_MOTOR_REAR_RIGHT_C1_IDX, direction, duty);
        break;
    default:
        break;
    }
}

void board_motor_set_signed_duty(board_motor_wheel_t wheel, float duty_percent)
{
    if (duty_percent > 100.0f) {
        duty_percent = 100.0f;
    } else if (duty_percent < -100.0f) {
        duty_percent = -100.0f;
    }

    if (duty_percent > 0.0f) {
        board_motor_set(wheel, BOARD_MOTOR_DIRECTION_FORWARD, (uint8_t)duty_percent);
    } else if (duty_percent < 0.0f) {
        board_motor_set(wheel, BOARD_MOTOR_DIRECTION_REVERSE, (uint8_t)(-duty_percent));
    } else {
        board_motor_set(wheel, BOARD_MOTOR_DIRECTION_STOP, 0U);
    }
}
