#ifndef BOARD_OLED_H
#define BOARD_OLED_H

#include <stdint.h>

typedef enum {
    BOARD_OLED_STATUS_OK = 0,
    BOARD_OLED_STATUS_ARGUMENT,
    BOARD_OLED_STATUS_TIMEOUT,
    BOARD_OLED_STATUS_BUS_ERROR
} board_oled_status_t;

board_oled_status_t board_oled_init(void);
board_oled_status_t board_oled_clear(void);
board_oled_status_t board_oled_update(void);
board_oled_status_t board_oled_set_cursor(uint8_t column, uint8_t page);
board_oled_status_t board_oled_write_char(char value);
board_oled_status_t board_oled_write_string(const char *text);

#endif
