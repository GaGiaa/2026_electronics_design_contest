#include "app_bno055.h"

#include <string.h>

#define APP_BNO055_UART_START_BYTE 0xAAU
#define APP_BNO055_UART_WRITE_CMD 0x00U
#define APP_BNO055_UART_READ_CMD 0x01U
#define APP_BNO055_UART_READ_OK 0xBBU
#define APP_BNO055_UART_ACK 0xEEU
#define APP_BNO055_UART_WRITE_OK 0x01U
#define APP_BNO055_READ_ATTEMPTS 2U
#define APP_BNO055_RECOVERY_DELAY_MS 2U

#define APP_BNO055_REG_ACCEL_DATA_X 0x08U
#define APP_BNO055_REG_CHIP_ID 0x00U
#define APP_BNO055_REG_TEMP 0x34U
#define APP_BNO055_REG_SYS_STATUS 0x39U
#define APP_BNO055_REG_UNIT_SEL 0x3BU
#define APP_BNO055_REG_OPR_MODE 0x3DU
#define APP_BNO055_REG_PWR_MODE 0x3EU

#define APP_BNO055_CHIP_ID_VALUE 0xA0U
#define APP_BNO055_OPR_MODE_CONFIG 0x00U
#define APP_BNO055_OPR_MODE_IMU 0x08U
#define APP_BNO055_PWR_MODE_NORMAL 0x00U
#define APP_BNO055_UNIT_SEL_VALUE 0x00U

#define APP_BNO055_ACCELERATION_LSB_PER_MPS2 100.0f
#define APP_BNO055_GRAVITY_MPS2 9.80665f
#define APP_BNO055_GYRO_LSB_PER_DPS 16.0f
#define APP_BNO055_EULER_LSB_PER_DEG 16.0f

static int16_t app_bno055_read_i16_le(const uint8_t *data)
{
    return (int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
}

static void app_bno055_delay(const app_bno055_t *imu, uint32_t delay_ms)
{
    if (imu != NULL && imu->delay != NULL) {
        imu->delay(imu->context, delay_ms);
    }
}

static void app_bno055_set_error(app_bno055_t *imu, int error, app_bno055_stage_t stage)
{
    if (imu == NULL) {
        return;
    }

    imu->initialized = 0;
    imu->poll_step = 0U;
    imu->snapshot.online = 0;
    imu->snapshot.valid = 0;
    imu->snapshot.last_error = error;
    imu->snapshot.last_stage = (uint8_t)stage;
}

static void app_bno055_clear_error(app_bno055_t *imu)
{
    imu->snapshot.last_error = APP_BNO055_OK;
    imu->snapshot.last_stage = (uint8_t)APP_BNO055_STAGE_NONE;
    imu->snapshot.last_detail = (uint8_t)APP_BNO055_DETAIL_NONE;
    imu->snapshot.transport_status = 0;
    imu->snapshot.hal_error_code = 0U;
}

static int app_bno055_drain_bus(app_bno055_t *imu)
{
    if (imu == NULL || imu->drain == NULL) {
        return 1;
    }

    return imu->drain(imu->context, imu->timeout_ms);
}

static int app_bno055_write_register(app_bno055_t *imu, uint8_t reg, uint8_t value)
{
    uint8_t command[5];
    uint8_t response[2] = {0U, 0U};

    if (imu == NULL || imu->write == NULL || imu->read == NULL) {
        return APP_BNO055_ERR_ARGUMENT;
    }

    command[0] = APP_BNO055_UART_START_BYTE;
    command[1] = APP_BNO055_UART_WRITE_CMD;
    command[2] = reg;
    command[3] = 1U;
    command[4] = value;
    imu->snapshot.last_detail = (uint8_t)APP_BNO055_DETAIL_WRITE_COMMAND;
    if (!imu->write(imu->context, command, sizeof(command), imu->timeout_ms)) {
        return APP_BNO055_ERR_TRANSPORT;
    }
    imu->snapshot.last_detail = (uint8_t)APP_BNO055_DETAIL_WRITE_ACK;
    if (!imu->read(imu->context, response, sizeof(response), imu->timeout_ms)) {
        imu->snapshot.last_rx0 = response[0];
        imu->snapshot.last_rx1 = response[1];
        return APP_BNO055_ERR_TRANSPORT;
    }
    imu->snapshot.last_rx0 = response[0];
    imu->snapshot.last_rx1 = response[1];
    if (response[0] != APP_BNO055_UART_ACK || response[1] != APP_BNO055_UART_WRITE_OK) {
        imu->snapshot.last_detail = (uint8_t)APP_BNO055_DETAIL_WRITE_ACK_VALUE;
        (void)app_bno055_drain_bus(imu);
        return APP_BNO055_ERR_RESPONSE;
    }

    return APP_BNO055_OK;
}

static int app_bno055_read_registers(app_bno055_t *imu, uint8_t reg, uint8_t *data, uint8_t length)
{
    uint8_t command[4];
    uint8_t header[2] = {0U, 0U};
    uint8_t attempt;
    int recovered;

    if (imu == NULL || data == NULL || length == 0U || imu->write == NULL || imu->read == NULL) {
        return APP_BNO055_ERR_ARGUMENT;
    }

    command[0] = APP_BNO055_UART_START_BYTE;
    command[1] = APP_BNO055_UART_READ_CMD;
    command[2] = reg;
    command[3] = length;
    for (attempt = 0U; attempt < APP_BNO055_READ_ATTEMPTS; ++attempt) {
        (void)app_bno055_drain_bus(imu);
        imu->snapshot.last_detail = (uint8_t)APP_BNO055_DETAIL_READ_COMMAND;
        if (!imu->write(imu->context, command, sizeof(command), imu->timeout_ms)) {
            return APP_BNO055_ERR_TRANSPORT;
        }
        imu->snapshot.last_detail = (uint8_t)APP_BNO055_DETAIL_READ_HEADER;
        if (!imu->read(imu->context, header, sizeof(header), imu->timeout_ms)) {
            imu->snapshot.last_rx0 = header[0];
            imu->snapshot.last_rx1 = header[1];
            return APP_BNO055_ERR_TRANSPORT;
        }
        imu->snapshot.last_rx0 = header[0];
        imu->snapshot.last_rx1 = header[1];
        if (header[0] == APP_BNO055_UART_READ_OK && header[1] == length) {
            break;
        }
        imu->snapshot.last_detail = (uint8_t)APP_BNO055_DETAIL_READ_HEADER_VALUE;
        recovered = app_bno055_drain_bus(imu);
        if (header[0] != APP_BNO055_UART_ACK || !recovered ||
            attempt + 1U >= APP_BNO055_READ_ATTEMPTS) {
            return APP_BNO055_ERR_RESPONSE;
        }
        app_bno055_delay(imu, APP_BNO055_RECOVERY_DELAY_MS);
    }
    imu->snapshot.last_detail = (uint8_t)APP_BNO055_DETAIL_READ_PAYLOAD;
    if (!imu->read(imu->context, data, length, imu->timeout_ms)) {
        return APP_BNO055_ERR_TRANSPORT;
    }

    return APP_BNO055_OK;
}

static int app_bno055_parse_sample(app_bno055_t *imu)
{
    uint32_t index;

    for (index = 0U; index < 3U; ++index) {
        imu->snapshot.acceleration_raw[index] = app_bno055_read_i16_le(&imu->sensor_data[index * 2U]);
        imu->snapshot.acceleration_g[index] =
            (float)imu->snapshot.acceleration_raw[index] / APP_BNO055_ACCELERATION_LSB_PER_MPS2 /
            APP_BNO055_GRAVITY_MPS2;
        imu->snapshot.angular_rate_raw[index] = app_bno055_read_i16_le(&imu->sensor_data[12U + index * 2U]);
        imu->snapshot.angular_rate_dps[index] =
            (float)imu->snapshot.angular_rate_raw[index] / APP_BNO055_GYRO_LSB_PER_DPS;
    }
    imu->snapshot.angle_raw[0] = app_bno055_read_i16_le(&imu->sensor_data[20U]);
    imu->snapshot.angle_raw[1] = app_bno055_read_i16_le(&imu->sensor_data[22U]);
    imu->snapshot.angle_raw[2] = app_bno055_read_i16_le(&imu->sensor_data[18U]);
    imu->snapshot.roll_deg = (float)imu->snapshot.angle_raw[0] / APP_BNO055_EULER_LSB_PER_DEG;
    imu->snapshot.pitch_deg = (float)imu->snapshot.angle_raw[1] / APP_BNO055_EULER_LSB_PER_DEG;
    imu->snapshot.yaw_deg = (float)imu->snapshot.angle_raw[2] / APP_BNO055_EULER_LSB_PER_DEG;
    imu->snapshot.temperature_raw = (int8_t)imu->temperature_calibration[0];
    imu->snapshot.temperature_celsius = (float)imu->snapshot.temperature_raw;
    imu->snapshot.calibration = app_bno055_decode_calibration(imu->temperature_calibration[1]);
    imu->snapshot.sys_status = imu->system_status[0];
    imu->snapshot.sys_err = imu->system_status[1];
    return APP_BNO055_OK;
}

void app_bno055_init(app_bno055_t *imu,
                     void *context,
                     app_bno055_write_fn write_fn,
                     app_bno055_read_fn read_fn,
                     app_bno055_delay_fn delay_fn,
                     uint32_t timeout_ms)
{
    if (imu == NULL) {
        return;
    }

    memset(imu, 0, sizeof(*imu));
    imu->context = context;
    imu->write = write_fn;
    imu->read = read_fn;
    imu->delay = delay_fn;
    imu->timeout_ms = timeout_ms;
}

void app_bno055_reset(app_bno055_t *imu)
{
    uint32_t complete_sample_count;

    if (imu == NULL) {
        return;
    }

    complete_sample_count = imu->snapshot.complete_sample_count;
    imu->initialized = 0;
    imu->poll_step = 0U;
    memset(imu->sensor_data, 0, sizeof(imu->sensor_data));
    memset(imu->temperature_calibration, 0, sizeof(imu->temperature_calibration));
    memset(imu->system_status, 0, sizeof(imu->system_status));
    memset(&imu->snapshot, 0, sizeof(imu->snapshot));
    /* Keep the runtime sample total monotonic across automatic reconnects. */
    imu->snapshot.complete_sample_count = complete_sample_count;
}

void app_bno055_set_drain(app_bno055_t *imu, app_bno055_drain_fn drain_fn)
{
    if (imu != NULL) {
        imu->drain = drain_fn;
    }
}

void app_bno055_set_transport_diagnostics(app_bno055_t *imu,
                                          int transport_status,
                                          uint32_t hal_error_code)
{
    if (imu != NULL) {
        imu->snapshot.transport_status = transport_status;
        imu->snapshot.hal_error_code = hal_error_code;
    }
}

app_bno055_calibration_t app_bno055_decode_calibration(uint8_t raw)
{
    app_bno055_calibration_t calibration;

    calibration.sys = (uint8_t)((raw >> 6) & 0x03U);
    calibration.gyro = (uint8_t)((raw >> 4) & 0x03U);
    calibration.acc = (uint8_t)((raw >> 2) & 0x03U);
    calibration.mag = (uint8_t)(raw & 0x03U);
    return calibration;
}

int app_bno055_start(app_bno055_t *imu)
{
    uint8_t chip_id = 0U;
    int result;

    if (imu == NULL) {
        return APP_BNO055_ERR_ARGUMENT;
    }

    result = app_bno055_read_registers(imu, APP_BNO055_REG_CHIP_ID, &chip_id, 1U);
    if (result != APP_BNO055_OK) {
        app_bno055_set_error(imu, result, APP_BNO055_STAGE_CHIP_ID);
        return result;
    }
    if (chip_id != APP_BNO055_CHIP_ID_VALUE) {
        app_bno055_set_error(imu, APP_BNO055_ERR_CHIP_ID, APP_BNO055_STAGE_CHIP_ID);
        return APP_BNO055_ERR_CHIP_ID;
    }

    result = app_bno055_write_register(imu, APP_BNO055_REG_OPR_MODE,
                                        APP_BNO055_OPR_MODE_CONFIG);
    if (result != APP_BNO055_OK) {
        app_bno055_set_error(imu, result, APP_BNO055_STAGE_OPR_MODE_CONFIG);
        return result;
    }
    app_bno055_delay(imu, 20U);

    result = app_bno055_write_register(imu, APP_BNO055_REG_PWR_MODE,
                                        APP_BNO055_PWR_MODE_NORMAL);
    if (result != APP_BNO055_OK) {
        app_bno055_set_error(imu, result, APP_BNO055_STAGE_PWR_MODE);
        return result;
    }
    app_bno055_delay(imu, 10U);

    result = app_bno055_write_register(imu, APP_BNO055_REG_UNIT_SEL,
                                        APP_BNO055_UNIT_SEL_VALUE);
    if (result != APP_BNO055_OK) {
        app_bno055_set_error(imu, result, APP_BNO055_STAGE_UNIT_SEL);
        return result;
    }
    app_bno055_delay(imu, 10U);

    result = app_bno055_write_register(imu, APP_BNO055_REG_OPR_MODE,
                                        APP_BNO055_OPR_MODE_IMU);
    if (result != APP_BNO055_OK) {
        app_bno055_set_error(imu, result, APP_BNO055_STAGE_OPR_MODE_IMU);
        return result;
    }
    app_bno055_delay(imu, 10U);
    imu->initialized = 1;
    imu->poll_step = 0U;
    imu->snapshot.online = 1;
    imu->snapshot.valid = 0;
    app_bno055_clear_error(imu);
    return APP_BNO055_OK;
}

int app_bno055_poll_step(app_bno055_t *imu)
{
    int result;

    if (imu == NULL || !imu->initialized) {
        return APP_BNO055_ERR_ARGUMENT;
    }
    if (imu->poll_step == 0U) {
        result = app_bno055_read_registers(imu, APP_BNO055_REG_ACCEL_DATA_X,
                                           imu->sensor_data, sizeof(imu->sensor_data));
        if (result != APP_BNO055_OK) {
            app_bno055_set_error(imu, result, APP_BNO055_STAGE_SENSOR_DATA);
            return result;
        }
        imu->poll_step = 1U;
        return APP_BNO055_STEP_IN_PROGRESS;
    }
    if (imu->poll_step == 1U) {
        result = app_bno055_read_registers(imu, APP_BNO055_REG_TEMP,
                                           imu->temperature_calibration,
                                           sizeof(imu->temperature_calibration));
        if (result != APP_BNO055_OK) {
            app_bno055_set_error(imu, result, APP_BNO055_STAGE_TEMPERATURE_CALIBRATION);
            return result;
        }
        imu->poll_step = 2U;
        return APP_BNO055_STEP_IN_PROGRESS;
    }
    result = app_bno055_read_registers(imu, APP_BNO055_REG_SYS_STATUS,
                                       &imu->system_status[0], 1U);
    if (result != APP_BNO055_OK) {
        app_bno055_set_error(imu, result, APP_BNO055_STAGE_SYSTEM_STATUS);
        return result;
    }
    result = app_bno055_read_registers(imu, APP_BNO055_REG_SYS_STATUS + 1U,
                                       &imu->system_status[1], 1U);
    if (result != APP_BNO055_OK) {
        app_bno055_set_error(imu, result, APP_BNO055_STAGE_SYSTEM_STATUS);
        return result;
    }
    result = app_bno055_parse_sample(imu);
    if (result != APP_BNO055_OK) {
        app_bno055_set_error(imu, result, APP_BNO055_STAGE_PARSE);
        return result;
    }
    imu->poll_step = 0U;
    imu->snapshot.online = 1;
    imu->snapshot.valid = 1;
    imu->snapshot.complete_sample_count++;
    app_bno055_clear_error(imu);
    return APP_BNO055_OK;
}

const app_bno055_snapshot_t *app_bno055_get_snapshot(const app_bno055_t *imu)
{
    return imu == NULL ? NULL : &imu->snapshot;
}
