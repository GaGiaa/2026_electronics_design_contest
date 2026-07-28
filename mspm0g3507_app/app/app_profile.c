#include "app/app_profile.h"

#include "config/app_config.h"
#include "config/crsf_config.h"

static const app_profile_t g_app_profile = {
    .enable_motor_control = true,
    .enable_imu = (APP_IMU_YAW_ENABLE != 0U),
    .enable_grayscale = true,
    .enable_crsf = (CRSF_REMOTE_CONTROL_ENABLE != 0U),
    .enable_oled = (APP_OLED_TEST_TASK_ENABLE != 0U),
    .enable_telemetry = (APP_VOFA_SPEED_PID_TELEMETRY_ENABLE != 0U) ||
                        (APP_GRAY_VOFA_TELEMETRY_ENABLE != 0U) ||
                        (APP_LINE_CONTROL_VOFA_TELEMETRY_ENABLE != 0U) ||
                        (APP_COURSE_FOLLOWING_VOFA_TELEMETRY_ENABLE != 0U) ||
                        (APP_BUTTON_VOFA_TELEMETRY_ENABLE != 0U) ||
                        (APP_IMU_TELEMETRY_ENABLE != 0U),
};

const app_profile_t *app_profile_get(void)
{
    return &g_app_profile;
}
