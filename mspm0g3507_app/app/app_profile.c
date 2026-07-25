#include "app/app_profile.h"

static const app_profile_t g_app_profile = {
    .enable_motor_control = true,
    .enable_imu = true,
    .enable_grayscale = true,
    .enable_crsf = (CRSF_REMOTE_CONTROL_ENABLE != 0U),
    .enable_oled = (OLED_TEST_TASK_ENABLE != 0U),
    .enable_telemetry = (VOFA_SPEED_PID_TELEMETRY_ENABLE != 0U) ||
                        (GRAY_VOFA_TELEMETRY_ENABLE != 0U) ||
                        (IMU_TELEMETRY_ENABLE != 0U),
};

const app_profile_t *app_profile_get(void)
{
    return &g_app_profile;
}
