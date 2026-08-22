/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define TOF_SDA_Pin GPIO_PIN_0
#define TOF_SDA_GPIO_Port GPIOF
#define TOF_SCL_Pin GPIO_PIN_1
#define TOF_SCL_GPIO_Port GPIOF
#define TOF_LPn_Pin GPIO_PIN_2
#define TOF_LPn_GPIO_Port GPIOF
#define TOF_INT_Pin GPIO_PIN_3
#define TOF_INT_GPIO_Port GPIOF
#define TOF_INT_EXTI_IRQn EXTI3_IRQn
#define CHARGER_Pin GPIO_PIN_7
#define CHARGER_GPIO_Port GPIOA
#define USB_STATUS_Pin GPIO_PIN_4
#define USB_STATUS_GPIO_Port GPIOC
#define BATT_Pin GPIO_PIN_5
#define BATT_GPIO_Port GPIOC
#define WIFI_PWR_EN_Pin GPIO_PIN_11
#define WIFI_PWR_EN_GPIO_Port GPIOF
#define IMU_INT1_Pin GPIO_PIN_13
#define IMU_INT1_GPIO_Port GPIOF
#define IMU_INT1_EXTI_IRQn EXTI15_10_IRQn
#define IMU_SCL_Pin GPIO_PIN_14
#define IMU_SCL_GPIO_Port GPIOF
#define IMU_SDA_Pin GPIO_PIN_15
#define IMU_SDA_GPIO_Port GPIOF
#define LCD_CS_Pin GPIO_PIN_0
#define LCD_CS_GPIO_Port GPIOG
#define LCD_BL_Pin GPIO_PIN_1
#define LCD_BL_GPIO_Port GPIOG
#define IMU_INT2_Pin GPIO_PIN_7
#define IMU_INT2_GPIO_Port GPIOE
#define IMU_INT2_EXTI_IRQn EXTI9_5_IRQn
#define LCD_RST_Pin GPIO_PIN_13
#define LCD_RST_GPIO_Port GPIOE
#define WIFI_CS_Pin GPIO_PIN_12
#define WIFI_CS_GPIO_Port GPIOB
#define WIFI_BOOT_Pin GPIO_PIN_8
#define WIFI_BOOT_GPIO_Port GPIOD
#define LCD_SDA_Pin GPIO_PIN_9
#define LCD_SDA_GPIO_Port GPIOC
#define LCD_SCL_Pin GPIO_PIN_8
#define LCD_SCL_GPIO_Port GPIOA
#define DATA_LED_Pin GPIO_PIN_9
#define DATA_LED_GPIO_Port GPIOA
#define WIFI_EN_Pin GPIO_PIN_3
#define WIFI_EN_GPIO_Port GPIOB
#define WIFI_RDY_Pin GPIO_PIN_4
#define WIFI_RDY_GPIO_Port GPIOB
#define WIFI_RDY_EXTI_IRQn EXTI4_IRQn
#define I2S_SDMODE_Pin GPIO_PIN_6
#define I2S_SDMODE_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
