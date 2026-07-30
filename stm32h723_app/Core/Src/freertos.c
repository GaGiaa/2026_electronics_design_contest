/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "app_telemetry.h"
#include "app_chassis_service.h"
#include "app_config.h"
#include "app_jy901s_service.h"
#include "app_k230_service.h"
#include "app_grayscale_service.h"
#include "app_time.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

static osThreadId_t chassisTaskHandle;
static const osThreadAttr_t chassisTask_attributes = {
  .name = "chassisTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t)osPriorityHigh,
};

static osThreadId_t jy901sTaskHandle;
static const osThreadAttr_t jy901sTask_attributes = {
  .name = "jy901sTask",
  .stack_size = 1024 * 2,
  .priority = (osPriority_t)osPriorityAboveNormal,
};

static osThreadId_t grayscaleTaskHandle;
static const osThreadAttr_t grayscaleTask_attributes = {
  .name = "grayscaleTask",
  .stack_size = 1024 * 2,
  .priority = (osPriority_t)osPriorityAboveNormal,
};

#if (APP_H723_K230_UART2_TEST_ENABLE == 1U)
static osThreadId_t k230TaskHandle;
static const osThreadAttr_t k230Task_attributes = {
  .name = "k230Task",
  .stack_size = 1024 * 2,
  .priority = (osPriority_t)osPriorityAboveNormal,
};
#endif

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 2000 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

void startChassisTask(void *argument);
void startJy901sTask(void *argument);
void startGrayscaleTask(void *argument);
#if (APP_H723_K230_UART2_TEST_ENABLE == 1U)
void startK230Task(void *argument);
#endif

/* USER CODE END FunctionPrototypes */

void startDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
}
/* USER CODE END 4 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(startDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  chassisTaskHandle = osThreadNew(startChassisTask, NULL, &chassisTask_attributes);
  configASSERT(chassisTaskHandle != NULL);
  jy901sTaskHandle = osThreadNew(startJy901sTask, NULL, &jy901sTask_attributes);
  configASSERT(jy901sTaskHandle != NULL);
  grayscaleTaskHandle = osThreadNew(startGrayscaleTask, NULL, &grayscaleTask_attributes);
  configASSERT(grayscaleTaskHandle != NULL);
#if (APP_H723_K230_UART2_TEST_ENABLE == 1U)
  k230TaskHandle = osThreadNew(startK230Task, NULL, &k230Task_attributes);
  configASSERT(k230TaskHandle != NULL);
#endif
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_startDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_startDefaultTask */
__weak void startDefaultTask(void *argument)
{
  /* USER CODE BEGIN startDefaultTask */
  h723_app_telemetry_init();
  /* Infinite loop */
  for(;;)
  {
    h723_app_telemetry_step();
    osDelay(1);
  }
  /* USER CODE END startDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

void startChassisTask(void *argument)
{
  uint32_t next_wake_tick = osKernelGetTickCount();

  (void)argument;
  h723_chassis_service_init();
  for (;;) {
    h723_chassis_service_step(h723_app_time_now_ms());
    next_wake_tick += APP_H723_CHASSIS_TASK_PERIOD_MS;
    (void)osDelayUntil(next_wake_tick);
  }
}

void startJy901sTask(void *argument)
{
  uint32_t next_wake_tick = osKernelGetTickCount();

  (void)argument;
  h723_jy901s_service_init();
  for (;;) {
    h723_jy901s_service_step(h723_app_time_now_ms());
    next_wake_tick += APP_JY901S_TASK_PERIOD_MS;
    (void)osDelayUntil(next_wake_tick);
  }
}

void startGrayscaleTask(void *argument)
{
  uint32_t next_wake_tick = osKernelGetTickCount();

  (void)argument;
  h723_grayscale_service_init();
  for (;;) {
    h723_grayscale_service_step(h723_app_time_now_ms());
    next_wake_tick += APP_GRAYSCALE_TASK_PERIOD_MS;
    (void)osDelayUntil(next_wake_tick);
  }
}

#if (APP_H723_K230_UART2_TEST_ENABLE == 1U)
void startK230Task(void *argument)
{
  uint32_t next_wake_tick = osKernelGetTickCount();

  (void)argument;
  h723_k230_service_init();
  for (;;) {
    h723_k230_service_step(h723_app_time_now_ms());
    next_wake_tick += 5U;
    (void)osDelayUntil(next_wake_tick);
  }
}
#endif

/* USER CODE END Application */

