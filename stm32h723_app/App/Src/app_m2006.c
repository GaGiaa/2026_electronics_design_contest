#include "app_m2006.h"

bool app_m2006_parse_feedback(uint32_t standard_id, const uint8_t data[8], app_m2006_feedback_t *feedback)
{
    if (data == 0 || feedback == 0 || standard_id < 0x201U || standard_id > 0x202U) { return false; }
    feedback->motor_id = (uint8_t)(standard_id - 0x200U);
    feedback->encoder = (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
    feedback->speed_rpm = (int16_t)(((uint16_t)data[2] << 8U) | data[3]);
    feedback->current = (int16_t)(((uint16_t)data[4] << 8U) | data[5]);
    feedback->temperature_celsius = data[6];
    return true;
}

void app_m2006_encode_group_current(int16_t left_current, int16_t right_current, uint8_t data[8])
{
    data[0] = (uint8_t)((uint16_t)left_current >> 8U); data[1] = (uint8_t)left_current;
    data[2] = (uint8_t)((uint16_t)right_current >> 8U); data[3] = (uint8_t)right_current;
    data[4] = 0U; data[5] = 0U; data[6] = 0U; data[7] = 0U;
}
