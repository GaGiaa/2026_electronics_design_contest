#ifndef BOARD_HCSR04_H
#define BOARD_HCSR04_H

#include <stdbool.h>
#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

typedef struct {
    bool valid;
    uint32_t echo_ticks;
} board_hcsr04_result_t;

void board_hcsr04_init(void);
void board_hcsr04_set_task_handle(TaskHandle_t task_handle);
void board_hcsr04_trigger(void);
void board_hcsr04_abort(void);
bool board_hcsr04_read_result(board_hcsr04_result_t *result);
void board_hcsr04_gpio_irq_handler(void);

#endif
