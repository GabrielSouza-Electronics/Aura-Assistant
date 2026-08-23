#include "bsp_wifi.h"

#include "main.h"
#include "spi.h"

BSP_WIFI_Status_t BSP_WIFI_Init(void)
{
    if (HAL_SPI_GetState(&hspi2) == HAL_SPI_STATE_RESET)
    {
        MX_SPI2_Init();
    }
    if (HAL_SPI_GetState(&hspi2) != HAL_SPI_STATE_READY)
    {
        return BSP_WIFI_ERROR_SPI_INIT;
    }

    /* Reproduce the cold-boot sequence validated on the Aura PCB. */
    HAL_GPIO_WritePin(WIFI_PWR_EN_GPIO_Port, WIFI_PWR_EN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(WIFI_EN_GPIO_Port, WIFI_EN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(WIFI_BOOT_GPIO_Port, WIFI_BOOT_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(WIFI_CS_GPIO_Port, WIFI_CS_Pin, GPIO_PIN_RESET);
    HAL_Delay(100U);

    HAL_GPIO_WritePin(WIFI_PWR_EN_GPIO_Port, WIFI_PWR_EN_Pin, GPIO_PIN_SET);
    /* The module first asserted RDY about 808 ms after CHIP_EN on REV01.
       Give the 3V3_WIFI rail time to settle before the middleware raises it. */
    HAL_Delay(100U);

    return (HAL_GPIO_ReadPin(WIFI_PWR_EN_GPIO_Port, WIFI_PWR_EN_Pin) ==
            GPIO_PIN_SET) ? BSP_WIFI_OK : BSP_WIFI_ERROR_INVALID_STATE;
}

void BSP_WIFI_DeInit(void)
{
    HAL_GPIO_WritePin(WIFI_EN_GPIO_Port, WIFI_EN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(WIFI_PWR_EN_GPIO_Port, WIFI_PWR_EN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(WIFI_CS_GPIO_Port, WIFI_CS_Pin, GPIO_PIN_RESET);
}

bool BSP_WIFI_IsPowered(void)
{
    return HAL_GPIO_ReadPin(WIFI_PWR_EN_GPIO_Port, WIFI_PWR_EN_Pin) ==
           GPIO_PIN_SET;
}

bool BSP_WIFI_IsEnabled(void)
{
    return HAL_GPIO_ReadPin(WIFI_EN_GPIO_Port, WIFI_EN_Pin) == GPIO_PIN_SET;
}

bool BSP_WIFI_IsReady(void)
{
    return HAL_GPIO_ReadPin(WIFI_RDY_GPIO_Port, WIFI_RDY_Pin) == GPIO_PIN_SET;
}
