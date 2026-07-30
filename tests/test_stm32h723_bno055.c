#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "app_bno055.h"

#define MOCK_MAX_READS 16U
#define MOCK_MAX_BYTES 32U

typedef struct {
    uint8_t data[MOCK_MAX_BYTES];
    uint8_t length;
} mock_read_t;

typedef struct {
    uint8_t writes[MOCK_MAX_READS][5];
    uint8_t write_lengths[MOCK_MAX_READS];
    uint8_t write_count;
    mock_read_t reads[MOCK_MAX_READS];
    uint8_t read_count;
    uint8_t read_index;
    uint32_t delays[MOCK_MAX_READS];
    uint8_t delay_count;
    int fail_read;
} mock_uart_t;

static void put_i16_le(uint8_t *data, int16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)((uint16_t)value >> 8U);
}

static void push_read(mock_uart_t *mock, const uint8_t *data, uint8_t length)
{
    assert(mock->read_count < MOCK_MAX_READS);
    assert(length <= MOCK_MAX_BYTES);
    memcpy(mock->reads[mock->read_count].data, data, length);
    mock->reads[mock->read_count].length = length;
    mock->read_count++;
}

static int mock_write(void *context, const uint8_t *data, size_t length, uint32_t timeout_ms)
{
    mock_uart_t *mock = context;

    (void)timeout_ms;
    assert(length <= sizeof(mock->writes[0]));
    assert(mock->write_count < MOCK_MAX_READS);
    memcpy(mock->writes[mock->write_count], data, length);
    mock->write_lengths[mock->write_count] = (uint8_t)length;
    mock->write_count++;
    return 1;
}

static int mock_read(void *context, uint8_t *data, size_t length, uint32_t timeout_ms)
{
    mock_uart_t *mock = context;
    mock_read_t *response;

    (void)timeout_ms;
    if (mock->fail_read != 0 || mock->read_index >= mock->read_count) {
        return 0;
    }
    response = &mock->reads[mock->read_index++];
    assert(response->length == length);
    memcpy(data, response->data, length);
    return 1;
}

static void mock_delay(void *context, uint32_t delay_ms)
{
    mock_uart_t *mock = context;

    assert(mock->delay_count < MOCK_MAX_READS);
    mock->delays[mock->delay_count++] = delay_ms;
}

static void push_read_response(mock_uart_t *mock, const uint8_t *data, uint8_t length)
{
    const uint8_t header[2] = { 0xBBU, length };

    push_read(mock, header, sizeof(header));
    push_read(mock, data, length);
}

static void push_write_ack(mock_uart_t *mock)
{
    const uint8_t ack[2] = { 0xEEU, 0x01U };

    push_read(mock, ack, sizeof(ack));
}

static void prepare_successful_start(mock_uart_t *mock)
{
    uint8_t chip_id = 0xA0U;

    push_read_response(mock, &chip_id, 1U);
    push_write_ack(mock);
    push_write_ack(mock);
    push_write_ack(mock);
    push_write_ack(mock);
}

static void test_start_configures_imu_through_required_mode_sequence(void)
{
    app_bno055_t imu;
    mock_uart_t mock = {0};

    prepare_successful_start(&mock);
    app_bno055_init(&imu, &mock, mock_write, mock_read, mock_delay, 5U);

    assert(app_bno055_start(&imu) == APP_BNO055_OK);
    assert(mock.write_count == 5U);
    assert(memcmp(mock.writes[1], (const uint8_t[]){0xAAU, 0x00U, 0x3DU, 0x01U, 0x00U}, 5U) == 0);
    assert(memcmp(mock.writes[2], (const uint8_t[]){0xAAU, 0x00U, 0x3EU, 0x01U, 0x00U}, 5U) == 0);
    assert(memcmp(mock.writes[3], (const uint8_t[]){0xAAU, 0x00U, 0x3BU, 0x01U, 0x00U}, 5U) == 0);
    assert(memcmp(mock.writes[4], (const uint8_t[]){0xAAU, 0x00U, 0x3DU, 0x01U, 0x08U}, 5U) == 0);
    assert(mock.delay_count == 4U);
    assert(mock.delays[0] == 20U);
    assert(mock.delays[1] == 10U);
    assert(mock.delays[2] == 10U);
    assert(mock.delays[3] == 10U);
    assert(imu.snapshot.online != 0);
}

static void test_poll_publishes_jy_compatible_ten_channel_sample(void)
{
    app_bno055_t imu;
    mock_uart_t mock = {0};
    uint8_t sensor_data[24] = {0};
    const uint8_t temperature_calibration[] = { 26U, 0xE4U };
    const uint8_t system_status = 0x05U;
    const uint8_t system_error = 0x00U;

    prepare_successful_start(&mock);
    put_i16_le(&sensor_data[0], 981);
    put_i16_le(&sensor_data[2], -490);
    put_i16_le(&sensor_data[4], 0);
    put_i16_le(&sensor_data[12], 160);
    put_i16_le(&sensor_data[14], -80);
    put_i16_le(&sensor_data[16], 0);
    put_i16_le(&sensor_data[18], 1440);
    put_i16_le(&sensor_data[20], -480);
    put_i16_le(&sensor_data[22], 160);
    push_read(&mock, (const uint8_t[]){0xEEU, 0x07U}, 2U);
    push_read_response(&mock, sensor_data, sizeof(sensor_data));
    push_read_response(&mock, temperature_calibration, sizeof(temperature_calibration));
    push_read_response(&mock, &system_status, 1U);
    push_read_response(&mock, &system_error, 1U);
    app_bno055_init(&imu, &mock, mock_write, mock_read, mock_delay, 5U);
    assert(app_bno055_start(&imu) == APP_BNO055_OK);

    assert(app_bno055_poll_step(&imu) == APP_BNO055_STEP_IN_PROGRESS);
    assert(app_bno055_poll_step(&imu) == APP_BNO055_OK);
    assert(imu.snapshot.complete_sample_count == 1U);
    assert(mock.read_index == 13U);
    assert(imu.snapshot.valid != 0);
    assert(fabsf(imu.snapshot.acceleration_g[0] - 1.00034f) < 0.001f);
    assert(fabsf(imu.snapshot.acceleration_g[1] + 0.49966f) < 0.001f);
    assert(fabsf(imu.snapshot.angular_rate_dps[0] - 10.0f) < 0.001f);
    assert(fabsf(imu.snapshot.roll_deg + 30.0f) < 0.001f);
    assert(fabsf(imu.snapshot.pitch_deg - 10.0f) < 0.001f);
    assert(fabsf(imu.snapshot.yaw_deg - 90.0f) < 0.001f);
    assert(imu.snapshot.temperature_raw == 26);
    assert(imu.snapshot.temperature_celsius == 26.0f);
    assert(imu.snapshot.calibration.sys == 3U);
    assert(imu.snapshot.calibration.gyro == 2U);
    assert(imu.snapshot.calibration.acc == 1U);
    assert(imu.snapshot.calibration.mag == 0U);
    assert(imu.snapshot.sys_status == 0x05U);
    assert(imu.snapshot.sys_err == 0U);
}

static void test_start_rejects_error_ack_and_read_timeout(void)
{
    app_bno055_t imu;
    mock_uart_t mock = {0};
    const uint8_t chip_id = 0xA0U;
    const uint8_t error_ack[] = { 0xEEU, 0x03U };

    push_read_response(&mock, &chip_id, 1U);
    push_read(&mock, error_ack, sizeof(error_ack));
    app_bno055_init(&imu, &mock, mock_write, mock_read, mock_delay, 5U);
    assert(app_bno055_start(&imu) == APP_BNO055_ERR_RESPONSE);
    assert(imu.snapshot.online == 0);
    assert(imu.snapshot.last_error == APP_BNO055_ERR_RESPONSE);

    memset(&mock, 0, sizeof(mock));
    mock.fail_read = 1;
    app_bno055_init(&imu, &mock, mock_write, mock_read, mock_delay, 5U);
    assert(app_bno055_start(&imu) == APP_BNO055_ERR_TRANSPORT);
    assert(imu.snapshot.last_error == APP_BNO055_ERR_TRANSPORT);
}

static void test_reset_preserves_sample_count_for_reconnect_diagnostics(void)
{
    app_bno055_t imu;

    memset(&imu, 0, sizeof(imu));
    imu.snapshot.complete_sample_count = 42U;
    app_bno055_reset(&imu);
    assert(imu.snapshot.complete_sample_count == 42U);
    assert(imu.snapshot.online == 0);
    assert(imu.snapshot.valid == 0);
}

int main(void)
{
    test_start_configures_imu_through_required_mode_sequence();
    test_poll_publishes_jy_compatible_ten_channel_sample();
    test_start_rejects_error_ack_and_read_timeout();
    test_reset_preserves_sample_count_for_reconnect_diagnostics();
    return 0;
}
