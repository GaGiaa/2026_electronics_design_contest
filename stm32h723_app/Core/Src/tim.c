/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    tim.c
  * @brief   This file provides code for the configuration
  *          of the TIM instances.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "tim.h"

TIM_HandleTypeDef htim2;

void MX_TIM2_Init(void)
{
    TIM_MasterConfigTypeDef master_config = {0};
    TIM_OC_InitTypeDef output_config = {0};

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 274U;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 499U;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) {
        Error_Handler();
    }

    master_config.MasterOutputTrigger = TIM_TRGO_RESET;
    master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &master_config) != HAL_OK) {
        Error_Handler();
    }

    output_config.OCMode = TIM_OCMODE_PWM1;
    output_config.Pulse = 0U;
    output_config.OCPolarity = TIM_OCPOLARITY_HIGH;
    output_config.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim2, &output_config, TIM_CHANNEL_4) != HAL_OK) {
        Error_Handler();
    }

    HAL_TIM_MspPostInit(&htim2);
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *tim_pwm_handle)
{
    if (tim_pwm_handle->Instance == TIM2) {
        __HAL_RCC_TIM2_CLK_ENABLE();
    }
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *tim_handle)
{
    GPIO_InitTypeDef gpio_init = {0};

    if (tim_handle->Instance == TIM2) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        gpio_init.Pin = GPIO_PIN_3;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
        gpio_init.Alternate = GPIO_AF1_TIM2;
        HAL_GPIO_Init(GPIOA, &gpio_init);
    }
}

void HAL_TIM_PWM_MspDeInit(TIM_HandleTypeDef *tim_pwm_handle)
{
    if (tim_pwm_handle->Instance == TIM2) {
        __HAL_RCC_TIM2_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_3);
    }
}
