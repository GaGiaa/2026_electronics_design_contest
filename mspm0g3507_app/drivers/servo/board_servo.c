#include "board_servo.h"

#include "drivers/servo/board_servo_math.h"
#include "ti_msp_dl_config.h"

static uint32_t pulse_us_to_timer_ticks(uint32_t pulse_us)
{
    return (uint32_t)(((uint64_t)pulse_us * SERVO_INST_CLK_FREQ) /
                      1000000ULL);
}

uint32_t board_servo_set_angle_deg(uint32_t angle_deg)
{
    const uint32_t pulse_us = board_servo_angle_to_pulse_us(angle_deg);
    const uint32_t compare_value = pulse_us_to_timer_ticks(pulse_us);

    DL_Timer_setCaptureCompareValue(SERVO_INST, compare_value,
                                    GPIO_SERVO_C0_IDX);
    return pulse_us;
}

void board_servo_init(void)
{
    (void)board_servo_set_angle_deg(APP_SERVO_INITIAL_ANGLE_DEG);
}
