#ifndef BOARD_BMI160_H
#define BOARD_BMI160_H

#include <stdint.h>

typedef enum {
    BOARD_BMI160_STATUS_OK = 0,
    BOARD_BMI160_STATUS_ARGUMENT,
    BOARD_BMI160_STATUS_TIMEOUT,
    BOARD_BMI160_STATUS_BUS,
    BOARD_BMI160_STATUS_CHIP_ID,
    BOARD_BMI160_STATUS_CONFIGURATION
} board_bmi160_status_t;

typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} board_bmi160_sample_t;

typedef struct {
    uint8_t chip_id;
    uint8_t error_register;
    uint8_t pmu_status;
    uint8_t accel_config;
    uint8_t accel_range;
    uint8_t gyro_config;
    uint8_t gyro_range;
    uint8_t init_status;
    uint8_t last_read_status;
    board_bmi160_sample_t last_sample;
} board_bmi160_diagnostics_t;

/* SWD-observable BMI160 bus and register diagnostics. Treat as read-only. */
extern volatile board_bmi160_diagnostics_t g_bmi160_diagnostics;

board_bmi160_status_t board_bmi160_init(uint8_t *chip_id);
board_bmi160_status_t board_bmi160_read_sample(board_bmi160_sample_t *sample);

#endif
