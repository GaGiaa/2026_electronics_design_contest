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

board_bmi160_status_t board_bmi160_init(uint8_t *chip_id);
board_bmi160_status_t board_bmi160_read_sample(board_bmi160_sample_t *sample);

#endif
