#ifndef APP_BNO055_H
#define APP_BNO055_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t sys;
    uint8_t gyro;
    uint8_t acc;
    uint8_t mag;
} app_bno055_calibration_t;

typedef struct {
    int online;
    int valid;
    int16_t acceleration_raw[3];
    int16_t angular_rate_raw[3];
    int16_t angle_raw[3];
    float acceleration_g[3];
    float angular_rate_dps[3];
    float roll_deg;
    float pitch_deg;
    float yaw_deg;
    int8_t temperature_raw;
    float temperature_celsius;
    app_bno055_calibration_t calibration;
    uint8_t sys_status;
    uint8_t sys_err;
    uint32_t complete_sample_count;
    int last_error;
    uint8_t last_stage;
    uint8_t last_detail;
    int transport_status;
    uint32_t hal_error_code;
    uint8_t last_rx0;
    uint8_t last_rx1;
} app_bno055_snapshot_t;

typedef int (*app_bno055_write_fn)(void *context,
                                   const uint8_t *data,
                                   size_t length,
                                   uint32_t timeout_ms);
typedef int (*app_bno055_read_fn)(void *context,
                                  uint8_t *data,
                                  size_t length,
                                  uint32_t timeout_ms);
typedef void (*app_bno055_delay_fn)(void *context, uint32_t delay_ms);
typedef int (*app_bno055_drain_fn)(void *context, uint32_t timeout_ms);

typedef struct {
    void *context;
    app_bno055_write_fn write;
    app_bno055_read_fn read;
    app_bno055_delay_fn delay;
    app_bno055_drain_fn drain;
    uint32_t timeout_ms;
    int initialized;
    uint8_t poll_step;
    uint8_t sensor_data[24];
    uint8_t temperature_calibration[2];
    uint8_t system_status[2];
    app_bno055_snapshot_t snapshot;
} app_bno055_t;

enum {
    APP_BNO055_OK = 0,
    APP_BNO055_STEP_IN_PROGRESS = 1,
    APP_BNO055_ERR_ARGUMENT = -1,
    APP_BNO055_ERR_TRANSPORT = -2,
    APP_BNO055_ERR_RESPONSE = -3,
    APP_BNO055_ERR_CHIP_ID = -4
};

typedef enum {
    APP_BNO055_STAGE_NONE = 0,
    APP_BNO055_STAGE_CHIP_ID = 1,
    APP_BNO055_STAGE_OPR_MODE_CONFIG = 2,
    APP_BNO055_STAGE_PWR_MODE = 3,
    APP_BNO055_STAGE_UNIT_SEL = 4,
    APP_BNO055_STAGE_OPR_MODE_IMU = 5,
    APP_BNO055_STAGE_SENSOR_DATA = 6,
    APP_BNO055_STAGE_TEMPERATURE_CALIBRATION = 7,
    APP_BNO055_STAGE_SYSTEM_STATUS = 8,
    APP_BNO055_STAGE_PARSE = 9
} app_bno055_stage_t;

typedef enum {
    APP_BNO055_DETAIL_NONE = 0,
    APP_BNO055_DETAIL_WRITE_COMMAND = 1,
    APP_BNO055_DETAIL_WRITE_ACK = 2,
    APP_BNO055_DETAIL_WRITE_ACK_VALUE = 3,
    APP_BNO055_DETAIL_READ_COMMAND = 4,
    APP_BNO055_DETAIL_READ_HEADER = 5,
    APP_BNO055_DETAIL_READ_PAYLOAD = 6,
    APP_BNO055_DETAIL_READ_HEADER_VALUE = 7
} app_bno055_detail_t;

void app_bno055_init(app_bno055_t *imu,
                     void *context,
                     app_bno055_write_fn write_fn,
                     app_bno055_read_fn read_fn,
                     app_bno055_delay_fn delay_fn,
                     uint32_t timeout_ms);
void app_bno055_reset(app_bno055_t *imu);
void app_bno055_set_drain(app_bno055_t *imu, app_bno055_drain_fn drain_fn);
void app_bno055_set_transport_diagnostics(app_bno055_t *imu,
                                          int transport_status,
                                          uint32_t hal_error_code);
app_bno055_calibration_t app_bno055_decode_calibration(uint8_t raw);
int app_bno055_start(app_bno055_t *imu);
int app_bno055_poll_step(app_bno055_t *imu);
const app_bno055_snapshot_t *app_bno055_get_snapshot(const app_bno055_t *imu);

#ifdef __cplusplus
}
#endif

#endif /* APP_BNO055_H */
