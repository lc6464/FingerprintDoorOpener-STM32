/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
typedef StaticQueue_t osStaticMessageQDef_t;
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

/* USER CODE END Variables */
/* Definitions for LEDTask */
osThreadId_t LEDTaskHandle;
uint32_t LEDTaskBuffer[128];
osStaticThreadDef_t LEDTaskControlBlock;
const osThreadAttr_t LEDTask_attributes = {
  .name = "LEDTask",
  .cb_mem = &LEDTaskControlBlock,
  .cb_size = sizeof(LEDTaskControlBlock),
  .stack_mem = &LEDTaskBuffer[0],
  .stack_size = sizeof(LEDTaskBuffer),
  .priority = (osPriority_t)osPriorityNormal,
};
/* Definitions for UARTTask */
osThreadId_t UARTTaskHandle;
uint32_t UARTTaskBuffer[512];
osStaticThreadDef_t UARTTaskControlBlock;
const osThreadAttr_t UARTTask_attributes = {
  .name = "UARTTask",
  .cb_mem = &UARTTaskControlBlock,
  .cb_size = sizeof(UARTTaskControlBlock),
  .stack_mem = &UARTTaskBuffer[0],
  .stack_size = sizeof(UARTTaskBuffer),
  .priority = (osPriority_t)osPriorityNormal,
};
/* Definitions for FPM383CTask */
osThreadId_t FPM383CTaskHandle;
uint32_t FPM383CTaskBuffer[512];
osStaticThreadDef_t FPM383CTaskControlBlock;
const osThreadAttr_t FPM383CTask_attributes = {
  .name = "FPM383CTask",
  .cb_mem = &FPM383CTaskControlBlock,
  .cb_size = sizeof(FPM383CTaskControlBlock),
  .stack_mem = &FPM383CTaskBuffer[0],
  .stack_size = sizeof(FPM383CTaskBuffer),
  .priority = (osPriority_t)osPriorityHigh,
};
/* Definitions for UARTQueue */
osMessageQueueId_t UARTQueueHandle;
uint32_t UARTQueueBuffer[16 * sizeof(uint32_t)];
osStaticMessageQDef_t UARTQueueControlBlock;
const osMessageQueueAttr_t UARTQueue_attributes = {
  .name = "UARTQueue",
  .cb_mem = &UARTQueueControlBlock,
  .cb_size = sizeof(UARTQueueControlBlock),
  .mq_mem = &UARTQueueBuffer,
  .mq_size = sizeof(UARTQueueBuffer)
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartLEDTask(void *argument);
void StartUARTTask(void *argument);
void StartFPM383CTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

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

  /* Create the queue(s) */
  /* creation of UARTQueue */
  UARTQueueHandle = osMessageQueueNew(16, sizeof(uint32_t), &UARTQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of LEDTask */
  LEDTaskHandle = osThreadNew(StartLEDTask, NULL, &LEDTask_attributes);

  /* creation of UARTTask */
  UARTTaskHandle = osThreadNew(StartUARTTask, NULL, &UARTTask_attributes);

  /* creation of FPM383CTask */
  FPM383CTaskHandle = osThreadNew(StartFPM383CTask, NULL, &FPM383CTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartLEDTask */
void LEDTask(void);
/**
  * @brief  Function implementing the LEDTask thread.
  * @param  argument: Not used
  * @retval None
  */
  /* USER CODE END Header_StartLEDTask */
void StartLEDTask(void *argument) {
  /* USER CODE BEGIN StartLEDTask */
  /* Infinite loop */
  LEDTask();
  /* USER CODE END StartLEDTask */
}

/* USER CODE BEGIN Header_StartUARTTask */
void UARTTask(void);
/**
* @brief Function implementing the UARTTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartUARTTask */
void StartUARTTask(void *argument) {
  /* USER CODE BEGIN StartUARTTask */
  /* Infinite loop */
  UARTTask();
  /* USER CODE END StartUARTTask */
}

/* USER CODE BEGIN Header_StartFPM383CTask */
void FPM383CTask(void);
/**
* @brief Function implementing the FPM383CTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartFPM383CTask */
void StartFPM383CTask(void *argument) {
  /* USER CODE BEGIN StartFPM383CTask */
  /* Infinite loop */
  FPM383CTask();
  /* USER CODE END StartFPM383CTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

