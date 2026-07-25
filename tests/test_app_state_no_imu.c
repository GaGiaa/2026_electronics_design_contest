#include "app/app_state.h"

volatile motor_control_wheel_status_t
    g_motor_control_status[BOARD_MOTOR_COUNT];

int main(void)
{
    app_state_init();
    app_state_encoder_cycle_begin();
    app_state_encoder_cycle_end();
    return g_encoder_sample_sequence == 2U ? 0 : 1;
}
