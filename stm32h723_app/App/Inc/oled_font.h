#ifndef OLED_FONT_H
#define OLED_FONT_H

#include <stdint.h>

#define BOARD_OLED_FONT_FIRST_ASCII 0x20U
#define BOARD_OLED_FONT_LAST_ASCII  0x7EU
#define BOARD_OLED_FONT_WIDTH       5U

extern const uint8_t g_board_oled_font[BOARD_OLED_FONT_LAST_ASCII -
                                       BOARD_OLED_FONT_FIRST_ASCII + 1U][BOARD_OLED_FONT_WIDTH];

#endif
