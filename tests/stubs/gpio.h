#ifndef TEST_STM32H723_GPIO_H
#define TEST_STM32H723_GPIO_H

#include <stdint.h>

typedef struct {
    uint32_t port_id;
} GPIO_TypeDef;

typedef uint32_t GPIO_PinState;

extern GPIO_TypeDef g_test_gpio_a;
extern GPIO_TypeDef g_test_gpio_c;
extern uint32_t g_test_gpio_a_levels;
extern uint32_t g_test_gpio_c_levels;

#define GPIOA (&g_test_gpio_a)
#define GPIOC (&g_test_gpio_c)
#define GPIO_PIN_4 (1U << 4U)
#define GPIO_PIN_5 (1U << 5U)
#define GPIO_PIN_6 (1U << 6U)
#define GPIO_PIN_RESET 0U
#define GPIO_PIN_SET 1U

static inline GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port,
                                             uint16_t pin)
{
    uint32_t levels = (port == GPIOA) ? g_test_gpio_a_levels
                                      : g_test_gpio_c_levels;
    return ((levels & pin) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

#endif
