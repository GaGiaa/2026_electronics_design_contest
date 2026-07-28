#include "board_bmi160.h"

#include <stdbool.h>

#include <FreeRTOS.h>
#include <task.h>

#include "ti_msp_dl_config.h"

#define BMI160_CHIP_ID_REGISTER 0x00U
#define BMI160_CHIP_ID_VALUE 0xD1U
#define BMI160_GYRO_DATA_REGISTER 0x0CU
#define BMI160_DATA_LENGTH 12U
#define BMI160_COMMAND_REGISTER 0x7EU
#define BMI160_SOFT_RESET_COMMAND 0xB6U
#define BMI160_ACCEL_NORMAL_COMMAND 0x11U
#define BMI160_GYRO_NORMAL_COMMAND 0x15U
#define BMI160_SPI_COMM_TEST_REGISTER 0x7FU
#define BMI160_ERROR_REGISTER 0x02U
#define BMI160_PMU_STATUS_REGISTER 0x03U
#define BMI160_ACCEL_CONFIG_REGISTER 0x40U
#define BMI160_ACCEL_RANGE_REGISTER 0x41U
#define BMI160_GYRO_CONFIG_REGISTER 0x42U
#define BMI160_GYRO_RANGE_REGISTER 0x43U
#define BMI160_CONFIG_200HZ_NORMAL_BANDWIDTH 0x29U
#define BMI160_ACCEL_RANGE_4G 0x05U
#define BMI160_GYRO_RANGE_500DPS 0x02U
#define BMI160_PMU_STATUS_ACCEL_NORMAL 0x10U
#define BMI160_PMU_STATUS_GYRO_NORMAL 0x04U
#define BMI160_PMU_STATUS_SENSOR_MODE_MASK 0x3CU
#define BMI160_ERROR_CODE_MASK 0x1EU
#define BMI160_REGISTER_WRITE_DELAY_MS 1U
#define BMI160_SPI_TIMEOUT_LOOPS 100000U

volatile board_bmi160_diagnostics_t g_bmi160_diagnostics;

static void chip_select(bool selected)
{
    if (selected) {
        DL_GPIO_clearPins(BMI160_CS_PORT, BMI160_CS_CS_PIN);
    } else {
        DL_GPIO_setPins(BMI160_CS_PORT, BMI160_CS_CS_PIN);
    }
}

static void drain_receive_fifo(void)
{
    while (!DL_SPI_isRXFIFOEmpty(SPI_BMI160_INST)) {
        (void)DL_SPI_receiveData8(SPI_BMI160_INST);
    }
}

static board_bmi160_status_t transfer_byte(uint8_t tx_data, uint8_t *rx_data)
{
    uint32_t timeout = BMI160_SPI_TIMEOUT_LOOPS;

    while (DL_SPI_isTXFIFOFull(SPI_BMI160_INST) && (timeout > 0U)) {
        --timeout;
    }
    if (timeout == 0U) {
        return BOARD_BMI160_STATUS_TIMEOUT;
    }

    DL_SPI_transmitData8(SPI_BMI160_INST, tx_data);
    timeout = BMI160_SPI_TIMEOUT_LOOPS;
    while (DL_SPI_isRXFIFOEmpty(SPI_BMI160_INST) && (timeout > 0U)) {
        --timeout;
    }
    if (timeout == 0U) {
        return BOARD_BMI160_STATUS_TIMEOUT;
    }

    *rx_data = DL_SPI_receiveData8(SPI_BMI160_INST);
    timeout = BMI160_SPI_TIMEOUT_LOOPS;
    while (DL_SPI_isBusy(SPI_BMI160_INST) && (timeout > 0U)) {
        --timeout;
    }
    if (timeout == 0U) {
        return BOARD_BMI160_STATUS_TIMEOUT;
    }

    return BOARD_BMI160_STATUS_OK;
}

static board_bmi160_status_t select_spi_interface(void)
{
    board_bmi160_status_t status;
    uint8_t ignored;

    drain_receive_fifo();
    chip_select(true);
    status = transfer_byte(0xFFU, &ignored);
    chip_select(false);
    if (status == BOARD_BMI160_STATUS_OK) {
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
    return status;
}

static board_bmi160_status_t read_registers(uint8_t register_address,
                                             uint8_t *data,
                                             uint8_t length)
{
    board_bmi160_status_t status;
    uint8_t ignored;
    uint8_t index;

    if ((data == NULL) || (length == 0U)) {
        return BOARD_BMI160_STATUS_ARGUMENT;
    }

    drain_receive_fifo();
    chip_select(true);
    status = transfer_byte((uint8_t)(register_address | 0x80U), &ignored);
    if (status == BOARD_BMI160_STATUS_OK) {
        for (index = 0U; index < length; ++index) {
            status = transfer_byte(0x00U, &data[index]);
            if (status != BOARD_BMI160_STATUS_OK) {
                break;
            }
        }
    }
    chip_select(false);
    return status;
}

static board_bmi160_status_t write_register(uint8_t register_address,
                                             uint8_t value)
{
    board_bmi160_status_t status;
    uint8_t ignored;

    drain_receive_fifo();
    chip_select(true);
    status = transfer_byte((uint8_t)(register_address & 0x7FU), &ignored);
    if (status == BOARD_BMI160_STATUS_OK) {
        status = transfer_byte(value, &ignored);
    }
    chip_select(false);
    if (status == BOARD_BMI160_STATUS_OK) {
        vTaskDelay(pdMS_TO_TICKS(BMI160_REGISTER_WRITE_DELAY_MS));
    }
    return status;
}

static board_bmi160_status_t verify_configuration(void)
{
    board_bmi160_status_t status;
    uint8_t error_status;
    uint8_t pmu_status;
    uint8_t accel_config;
    uint8_t accel_range;
    uint8_t gyro_config;
    uint8_t gyro_range;
    bool configuration_error;

    status = read_registers(BMI160_ERROR_REGISTER, &error_status, 1U);
    if (status != BOARD_BMI160_STATUS_OK) {
        return status;
    }
    g_bmi160_diagnostics.error_register = error_status;
    configuration_error = ((error_status & BMI160_ERROR_CODE_MASK) != 0U);

    status = read_registers(BMI160_PMU_STATUS_REGISTER, &pmu_status, 1U);
    if (status != BOARD_BMI160_STATUS_OK) {
        return status;
    }
    g_bmi160_diagnostics.pmu_status = pmu_status;
    status = read_registers(BMI160_ACCEL_CONFIG_REGISTER, &accel_config, 1U);
    if (status != BOARD_BMI160_STATUS_OK) {
        return status;
    }
    g_bmi160_diagnostics.accel_config = accel_config;
    status = read_registers(BMI160_ACCEL_RANGE_REGISTER, &accel_range, 1U);
    if (status != BOARD_BMI160_STATUS_OK) {
        return status;
    }
    g_bmi160_diagnostics.accel_range = accel_range;
    status = read_registers(BMI160_GYRO_CONFIG_REGISTER, &gyro_config, 1U);
    if (status != BOARD_BMI160_STATUS_OK) {
        return status;
    }
    g_bmi160_diagnostics.gyro_config = gyro_config;
    status = read_registers(BMI160_GYRO_RANGE_REGISTER, &gyro_range, 1U);
    if (status != BOARD_BMI160_STATUS_OK) {
        return status;
    }
    g_bmi160_diagnostics.gyro_range = gyro_range;

    if (configuration_error ||
        (pmu_status & BMI160_PMU_STATUS_SENSOR_MODE_MASK) !=
        (BMI160_PMU_STATUS_ACCEL_NORMAL | BMI160_PMU_STATUS_GYRO_NORMAL) ||
        (accel_config != BMI160_CONFIG_200HZ_NORMAL_BANDWIDTH) ||
        (accel_range != BMI160_ACCEL_RANGE_4G) ||
        (gyro_config != BMI160_CONFIG_200HZ_NORMAL_BANDWIDTH) ||
        (gyro_range != BMI160_GYRO_RANGE_500DPS)) {
        return BOARD_BMI160_STATUS_CONFIGURATION;
    }

    return BOARD_BMI160_STATUS_OK;
}

board_bmi160_status_t board_bmi160_init(uint8_t *chip_id)
{
    board_bmi160_status_t status;
    uint8_t id = 0U;
    uint8_t ignored;

    if (chip_id == NULL) {
        g_bmi160_diagnostics.init_status =
            (uint8_t)BOARD_BMI160_STATUS_ARGUMENT;
        return BOARD_BMI160_STATUS_ARGUMENT;
    }
    *chip_id = 0U;
    g_bmi160_diagnostics.chip_id = 0U;
    g_bmi160_diagnostics.error_register = 0U;
    g_bmi160_diagnostics.pmu_status = 0U;
    g_bmi160_diagnostics.accel_config = 0U;
    g_bmi160_diagnostics.accel_range = 0U;
    g_bmi160_diagnostics.gyro_config = 0U;
    g_bmi160_diagnostics.gyro_range = 0U;

    status = select_spi_interface();
    if (status != BOARD_BMI160_STATUS_OK) {
        g_bmi160_diagnostics.init_status = (uint8_t)status;
        return status;
    }

    status = read_registers(BMI160_CHIP_ID_REGISTER, &id, 1U);
    if (status != BOARD_BMI160_STATUS_OK) {
        g_bmi160_diagnostics.init_status = (uint8_t)status;
        return status;
    }
    *chip_id = id;
    g_bmi160_diagnostics.chip_id = id;
    if (id != BMI160_CHIP_ID_VALUE) {
        g_bmi160_diagnostics.init_status =
            (uint8_t)BOARD_BMI160_STATUS_CHIP_ID;
        return BOARD_BMI160_STATUS_CHIP_ID;
    }

    status = write_register(BMI160_COMMAND_REGISTER, BMI160_SOFT_RESET_COMMAND);
    if (status != BOARD_BMI160_STATUS_OK) {
        g_bmi160_diagnostics.init_status = (uint8_t)status;
        return status;
    }
    vTaskDelay(pdMS_TO_TICKS(10U));

    /* Soft reset restores the default bus state; select SPI again. */
    status = select_spi_interface();
    if (status != BOARD_BMI160_STATUS_OK) {
        g_bmi160_diagnostics.init_status = (uint8_t)status;
        return status;
    }

    status = read_registers(BMI160_SPI_COMM_TEST_REGISTER, &ignored, 1U);
    if (status != BOARD_BMI160_STATUS_OK) {
        g_bmi160_diagnostics.init_status = (uint8_t)status;
        return status;
    }

    status = write_register(BMI160_ACCEL_CONFIG_REGISTER,
                            BMI160_CONFIG_200HZ_NORMAL_BANDWIDTH);
    if (status != BOARD_BMI160_STATUS_OK) {
        g_bmi160_diagnostics.init_status = (uint8_t)status;
        return status;
    }
    status = write_register(BMI160_ACCEL_RANGE_REGISTER, BMI160_ACCEL_RANGE_4G);
    if (status != BOARD_BMI160_STATUS_OK) {
        g_bmi160_diagnostics.init_status = (uint8_t)status;
        return status;
    }
    status = write_register(BMI160_GYRO_CONFIG_REGISTER,
                            BMI160_CONFIG_200HZ_NORMAL_BANDWIDTH);
    if (status != BOARD_BMI160_STATUS_OK) {
        g_bmi160_diagnostics.init_status = (uint8_t)status;
        return status;
    }
    status = write_register(BMI160_GYRO_RANGE_REGISTER, BMI160_GYRO_RANGE_500DPS);
    if (status != BOARD_BMI160_STATUS_OK) {
        g_bmi160_diagnostics.init_status = (uint8_t)status;
        return status;
    }

    status = write_register(BMI160_COMMAND_REGISTER, BMI160_ACCEL_NORMAL_COMMAND);
    if (status != BOARD_BMI160_STATUS_OK) {
        g_bmi160_diagnostics.init_status = (uint8_t)status;
        return status;
    }
    vTaskDelay(pdMS_TO_TICKS(5U));

    status = write_register(BMI160_COMMAND_REGISTER, BMI160_GYRO_NORMAL_COMMAND);
    if (status != BOARD_BMI160_STATUS_OK) {
        g_bmi160_diagnostics.init_status = (uint8_t)status;
        return status;
    }
    vTaskDelay(pdMS_TO_TICKS(80U));

    status = verify_configuration();
    g_bmi160_diagnostics.init_status = (uint8_t)status;
    return status;
}

board_bmi160_status_t board_bmi160_read_sample(board_bmi160_sample_t *sample)
{
    uint8_t data[BMI160_DATA_LENGTH];
    board_bmi160_status_t status;

    if (sample == NULL) {
        g_bmi160_diagnostics.last_read_status =
            (uint8_t)BOARD_BMI160_STATUS_ARGUMENT;
        return BOARD_BMI160_STATUS_ARGUMENT;
    }

    status = read_registers(BMI160_GYRO_DATA_REGISTER, data, BMI160_DATA_LENGTH);
    if (status != BOARD_BMI160_STATUS_OK) {
        g_bmi160_diagnostics.last_read_status = (uint8_t)status;
        return status;
    }

    sample->gyro_x = (int16_t)(((uint16_t)data[1] << 8U) | data[0]);
    sample->gyro_y = (int16_t)(((uint16_t)data[3] << 8U) | data[2]);
    sample->gyro_z = (int16_t)(((uint16_t)data[5] << 8U) | data[4]);
    sample->accel_x = (int16_t)(((uint16_t)data[7] << 8U) | data[6]);
    sample->accel_y = (int16_t)(((uint16_t)data[9] << 8U) | data[8]);
    sample->accel_z = (int16_t)(((uint16_t)data[11] << 8U) | data[10]);
    g_bmi160_diagnostics.last_sample = *sample;
    g_bmi160_diagnostics.last_read_status = (uint8_t)status;
    return BOARD_BMI160_STATUS_OK;
}
