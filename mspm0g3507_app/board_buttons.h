#ifndef BOARD_BUTTONS_H
#define BOARD_BUTTONS_H

#include <stdint.h>

#define BOARD_BUTTON_COUNT 4U
#define BOARD_BUTTON_DEBOUNCE_SAMPLES 2U

typedef enum {
    BOARD_BUTTON_PA7 = 0,
    BOARD_BUTTON_PB12,
    BOARD_BUTTON_PA8,
    BOARD_BUTTON_PA30
} board_button_t;

typedef struct {
    uint32_t pressed_mask;
    uint32_t released_mask;
} board_buttons_events_t;

void board_buttons_init(void);
board_buttons_events_t board_buttons_scan(void);

#endif
