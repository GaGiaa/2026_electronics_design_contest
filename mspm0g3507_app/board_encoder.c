#include "board_encoder.h"

#include <stdbool.h>

#include "ti_msp_dl_config.h"

#define BOARD_ENCODER_PI 3.1415926f

static volatile int32_t g_total_counts[BOARD_MOTOR_COUNT];
static volatile int32_t g_interval_counts[BOARD_MOTOR_COUNT];

static bool pin_is_high(GPIO_Regs *port, uint32_t pin)
{
    return DL_GPIO_readPins(port, pin) != 0U;
}

static void record_a_phase_edge(board_motor_wheel_t wheel, GPIO_Regs *a_port,
                                uint32_t a_pin,
                                GPIO_Regs *b_port, uint32_t b_pin)
{
    int32_t direction;
    bool a_is_high = pin_is_high(a_port, a_pin);
    bool b_is_high = pin_is_high(b_port, b_pin);

    /* SysConfig configures each A phase for RISE_FALL interrupts. */
    direction = (a_is_high == b_is_high) ? -1 : 1;
    g_total_counts[wheel] += direction;
    g_interval_counts[wheel] += direction;
}

void board_encoder_init(void)
{
    uint32_t wheel;

    for (wheel = 0U; wheel < BOARD_MOTOR_COUNT; ++wheel) {
        g_total_counts[wheel] = 0;
        g_interval_counts[wheel] = 0;
    }
}

void board_encoder_gpioa_irq_handler(void)
{
    switch (DL_GPIO_getPendingInterrupt(GPIOA)) {
    case ENCODER_FRONT_LEFT_A_IIDX:
        record_a_phase_edge(BOARD_MOTOR_FRONT_LEFT, ENCODER_FRONT_LEFT_A_PORT,
                            ENCODER_FRONT_LEFT_A_PIN, ENCODER_FRONT_LEFT_B_PORT,
                            ENCODER_FRONT_LEFT_B_PIN);
        break;
    case ENCODER_FRONT_RIGHT_A_IIDX:
        record_a_phase_edge(BOARD_MOTOR_FRONT_RIGHT, ENCODER_FRONT_RIGHT_A_PORT,
                            ENCODER_FRONT_RIGHT_A_PIN, ENCODER_FRONT_RIGHT_B_PORT,
                            ENCODER_FRONT_RIGHT_B_PIN);
        break;
    case ENCODER_REAR_LEFT_A_IIDX:
        record_a_phase_edge(BOARD_MOTOR_REAR_LEFT, ENCODER_REAR_LEFT_A_PORT,
                            ENCODER_REAR_LEFT_A_PIN, ENCODER_REAR_LEFT_B_PORT,
                            ENCODER_REAR_LEFT_B_PIN);
        break;
    case ENCODER_REAR_RIGHT_A_IIDX:
        record_a_phase_edge(BOARD_MOTOR_REAR_RIGHT, ENCODER_REAR_RIGHT_A_PORT,
                            ENCODER_REAR_RIGHT_A_PIN, ENCODER_REAR_RIGHT_B_PORT,
                            ENCODER_REAR_RIGHT_B_PIN);
        break;
    default:
        break;
    }
}

board_encoder_sample_t board_encoder_sample(board_motor_wheel_t wheel)
{
    board_encoder_sample_t sample = {0};
    uint32_t interrupt_mask;

    if (wheel >= BOARD_MOTOR_COUNT) {
        return sample;
    }

    interrupt_mask = __get_PRIMASK();
    __disable_irq();
    sample.delta_counts = g_interval_counts[wheel];
    sample.total_counts = g_total_counts[wheel];
    g_interval_counts[wheel] = 0;
    __set_PRIMASK(interrupt_mask);

    sample.speed_mm_per_s = ((float)sample.delta_counts *
                              (float)BOARD_ENCODER_WHEEL_DIAMETER_MM *
                              BOARD_ENCODER_PI * 1000.0f) /
                             ((float)BOARD_ENCODER_COUNTS_PER_REVOLUTION *
                              (float)BOARD_ENCODER_SAMPLE_PERIOD_MS);
    return sample;
}
