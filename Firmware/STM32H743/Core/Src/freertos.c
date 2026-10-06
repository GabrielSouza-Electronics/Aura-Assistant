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
#include "FreeRTOS.h"
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_tasks.h"
#include "app_reminders.h"
#include "app.h"

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

/* USER CODE END Variables */
/* Definitions for SystemTask */
osThreadId_t SystemTaskHandle;
const osThreadAttr_t SystemTask_attributes = {
  .name = "SystemTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for TouchGFXTask */
osThreadId_t TouchGFXTaskHandle;
const osThreadAttr_t TouchGFXTask_attributes = {
  .name = "TouchGFXTask",
  .stack_size = 4096 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for AudioInputTask */
osThreadId_t AudioInputTaskHandle;
const osThreadAttr_t AudioInputTask_attributes = {
  .name = "AudioInputTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for WiFiTask */
osThreadId_t WiFiTaskHandle;
const osThreadAttr_t WiFiTask_attributes = {
  .name = "WiFiTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for SensorTask */
osThreadId_t SensorTaskHandle;
const osThreadAttr_t SensorTask_attributes = {
  .name = "SensorTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for PowerTask */
osThreadId_t PowerTaskHandle;
const osThreadAttr_t PowerTask_attributes = {
  .name = "PowerTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for LEDTask */
osThreadId_t LEDTaskHandle;
const osThreadAttr_t LEDTask_attributes = {
  .name = "LEDTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for LoggingTask */
osThreadId_t LoggingTaskHandle;
const osThreadAttr_t LoggingTask_attributes = {
  .name = "LoggingTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for AudioOutputTask */
osThreadId_t AudioOutputTaskHandle;
const osThreadAttr_t AudioOutputTask_attributes = {
  .name = "AudioOutputTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for WiFiReadySem */
osSemaphoreId_t WiFiReadySemHandle;
const osSemaphoreAttr_t WiFiReadySem_attributes = {
  .name = "WiFiReadySem"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void System_Task(void *argument);
extern void TouchGFX_Task(void *argument);
void AudioInput_Task(void *argument);
void WiFi_Task(void *argument);
void Sensor_Task(void *argument);
void Power_Task(void *argument);
void LED_Task(void *argument);
void Logging_Task(void *argument);
void AudioOutput_Task(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, char *pcTaskName);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, char *pcTaskName)
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
  APP_TasksInit();
  APP_RemindersInit();

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of WiFiReadySem */
  WiFiReadySemHandle = osSemaphoreNew(1, 0, &WiFiReadySem_attributes);

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
  /* creation of SystemTask */
  SystemTaskHandle = osThreadNew(System_Task, NULL, &SystemTask_attributes);

  /* creation of TouchGFXTask */
  TouchGFXTaskHandle = osThreadNew(TouchGFX_Task, NULL, &TouchGFXTask_attributes);

  /* creation of AudioInputTask */
  AudioInputTaskHandle = osThreadNew(AudioInput_Task, NULL, &AudioInputTask_attributes);

  /* creation of WiFiTask */
  WiFiTaskHandle = osThreadNew(WiFi_Task, NULL, &WiFiTask_attributes);

  /* creation of SensorTask */
  SensorTaskHandle = osThreadNew(Sensor_Task, NULL, &SensorTask_attributes);

  /* creation of PowerTask */
  PowerTaskHandle = osThreadNew(Power_Task, NULL, &PowerTask_attributes);

  /* creation of LEDTask */
  LEDTaskHandle = osThreadNew(LED_Task, NULL, &LEDTask_attributes);

  /* creation of LoggingTask */
  LoggingTaskHandle = osThreadNew(Logging_Task, NULL, &LoggingTask_attributes);

  /* creation of AudioOutputTask */
  AudioOutputTaskHandle = osThreadNew(AudioOutput_Task, NULL, &AudioOutputTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_System_Task */
/**
  * @brief  Function implementing the SystemTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_System_Task */
void System_Task(void *argument)
{
  /* USER CODE BEGIN System_Task */ 
  APP_SystemTask();
  /* USER CODE END System_Task */
}

/* USER CODE BEGIN Header_AudioInput_Task */
/**
* @brief Function implementing the AudioInputTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_AudioInput_Task */
void AudioInput_Task(void *argument)
{
  /* USER CODE BEGIN AudioInput_Task */
  APP_AudioInputTask();
  /* USER CODE END AudioInput_Task */
}

/* USER CODE BEGIN Header_WiFi_Task */
/**
* @brief Function implementing the WiFiTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_WiFi_Task */
void WiFi_Task(void *argument)
{
  /* USER CODE BEGIN WiFi_Task */
  APP_WiFiTask();
  /* USER CODE END WiFi_Task */
}

/* USER CODE BEGIN Header_Sensor_Task */
/**
* @brief Function implementing the SensorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Sensor_Task */
void Sensor_Task(void *argument)
{
  /* USER CODE BEGIN Sensor_Task */
  APP_SensorTask();
  /* USER CODE END Sensor_Task */
}

/* USER CODE BEGIN Header_Power_Task */
/**
* @brief Function implementing the PowerTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Power_Task */
void Power_Task(void *argument)
{
  /* USER CODE BEGIN Power_Task */
  APP_PowerTask();
  /* USER CODE END Power_Task */
}

/* USER CODE BEGIN Header_LED_Task */
/**
* @brief Function implementing the LEDTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_LED_Task */
void LED_Task(void *argument)
{
  /* USER CODE BEGIN LED_Task */
  APP_LEDTask();
  /* USER CODE END LED_Task */
}

/* USER CODE BEGIN Header_Logging_Task */
/**
* @brief Function implementing the LoggingTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Logging_Task */
void Logging_Task(void *argument)
{
  /* USER CODE BEGIN Logging_Task */
  /* No logging service yet: park the task instead of waking every tick. */
  for(;;)
  {
    osDelay(osWaitForever);
  }
  /* USER CODE END Logging_Task */
}

/* USER CODE BEGIN Header_AudioOutput_Task */
/**
* @brief Function implementing the AudioOutputTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_AudioOutput_Task */
void AudioOutput_Task(void *argument)
{
  /* USER CODE BEGIN AudioOutput_Task */
  APP_AudioOutputTask();
  /* USER CODE END AudioOutput_Task */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

