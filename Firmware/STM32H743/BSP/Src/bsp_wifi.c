#include "bsp_wifi.h"

#include "main.h"
#include "spi.h"

#include <string.h>

static volatile uint32_t bsp_wifi_ready_irq_count;
static uint32_t bsp_wifi_spi_state_before_init;
static uint32_t bsp_wifi_spi_state_after_init;
static uint32_t bsp_wifi_spi_error;
static uint32_t bsp_wifi_spi_reinit_count;
static volatile uint32_t bsp_wifi_spi_transfer_count;
static volatile uint32_t bsp_wifi_spi_last_hal_status;
static volatile uint32_t bsp_wifi_spi_last_length;
static volatile uint32_t bsp_wifi_cs_assert_count;
static volatile uint32_t bsp_wifi_cs_deassert_count;
static volatile uint32_t bsp_wifi_spi_engine_task_create_status;
static volatile uint32_t bsp_wifi_spi_engine_task_start_count;
static volatile uint32_t bsp_wifi_spi_engine_task_wake_count;
static volatile uint32_t bsp_wifi_spi_engine_last_event_bits;
static volatile uint32_t bsp_wifi_spi_engine_deinit_count;
static uint8_t bsp_wifi_spi_last_rx[8];

BSP_WIFI_Status_t BSP_WIFI_Init(void)
{
    bsp_wifi_spi_state_before_init = (uint32_t)HAL_SPI_GetState(&hspi2);
    if (HAL_SPI_GetState(&hspi2) == HAL_SPI_STATE_RESET)
    {
        MX_SPI2_Init();
        ++bsp_wifi_spi_reinit_count;
    }
    bsp_wifi_spi_state_after_init = (uint32_t)HAL_SPI_GetState(&hspi2);
    bsp_wifi_spi_error = HAL_SPI_GetError(&hspi2);
    if (HAL_SPI_GetState(&hspi2) != HAL_SPI_STATE_READY)
    {
        return BSP_WIFI_ERROR_SPI_INIT;
    }

    /* Mission mode and inactive ST SPI chip-select. The ST67 SPI transport
       asserts CS high; keep it low until the middleware owns the bus. */
    HAL_GPIO_WritePin(WIFI_EN_GPIO_Port, WIFI_EN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(WIFI_BOOT_GPIO_Port, WIFI_BOOT_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(WIFI_CS_GPIO_Port, WIFI_CS_Pin, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(WIFI_PWR_EN_GPIO_Port, WIFI_PWR_EN_Pin, GPIO_PIN_SET);
    bsp_wifi_ready_irq_count = 0U;
    bsp_wifi_spi_transfer_count = 0U;
    bsp_wifi_spi_last_hal_status = 0U;
    bsp_wifi_spi_last_length = 0U;
    bsp_wifi_cs_assert_count = 0U;
    bsp_wifi_cs_deassert_count = 0U;
    bsp_wifi_spi_engine_task_create_status = UINT32_MAX;
    bsp_wifi_spi_engine_task_start_count = 0U;
    bsp_wifi_spi_engine_task_wake_count = 0U;
    bsp_wifi_spi_engine_last_event_bits = 0U;
    bsp_wifi_spi_engine_deinit_count = 0U;
    memset(bsp_wifi_spi_last_rx, 0, sizeof(bsp_wifi_spi_last_rx));
    HAL_Delay(10U);

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

void BSP_WIFI_ReadyIRQObserved(void)
{
    ++bsp_wifi_ready_irq_count;
}

uint32_t BSP_WIFI_GetReadyIRQCount(void)
{
    return bsp_wifi_ready_irq_count;
}

uint32_t BSP_WIFI_GetSPIStateBeforeInit(void)
{
    return bsp_wifi_spi_state_before_init;
}

uint32_t BSP_WIFI_GetSPIStateAfterInit(void)
{
    return bsp_wifi_spi_state_after_init;
}

uint32_t BSP_WIFI_GetSPIError(void)
{
    return bsp_wifi_spi_error;
}

uint32_t BSP_WIFI_GetSPIReinitCount(void)
{
    return bsp_wifi_spi_reinit_count;
}

void BSP_WIFI_SPITransferObserved(uint32_t hal_status,
                                  const void *rx_data,
                                  uint32_t length)
{
    uint32_t copy_length = length;
    if (copy_length > sizeof(bsp_wifi_spi_last_rx))
    {
        copy_length = sizeof(bsp_wifi_spi_last_rx);
    }
    bsp_wifi_spi_last_hal_status = hal_status;
    bsp_wifi_spi_last_length = length;
    ++bsp_wifi_spi_transfer_count;
    if ((rx_data != NULL) && (copy_length > 0U))
    {
        memcpy(bsp_wifi_spi_last_rx, rx_data, copy_length);
    }
}

void BSP_WIFI_CSObserved(bool asserted)
{
    if (asserted)
    {
        ++bsp_wifi_cs_assert_count;
    }
    else
    {
        ++bsp_wifi_cs_deassert_count;
    }
}

uint32_t BSP_WIFI_GetSPITransferCount(void)
{
    return bsp_wifi_spi_transfer_count;
}

uint32_t BSP_WIFI_GetSPILastHALStatus(void)
{
    return bsp_wifi_spi_last_hal_status;
}

uint32_t BSP_WIFI_GetSPILastLength(void)
{
    return bsp_wifi_spi_last_length;
}

uint32_t BSP_WIFI_GetCSAssertCount(void)
{
    return bsp_wifi_cs_assert_count;
}

uint32_t BSP_WIFI_GetCSDeassertCount(void)
{
    return bsp_wifi_cs_deassert_count;
}

void BSP_WIFI_GetSPILastRX(uint8_t destination[8])
{
    if (destination != NULL)
    {
        memcpy(destination, bsp_wifi_spi_last_rx,
               sizeof(bsp_wifi_spi_last_rx));
    }
}

void BSP_WIFI_SPIEngineTaskCreated(uint32_t create_status)
{
    bsp_wifi_spi_engine_task_create_status = create_status;
}

void BSP_WIFI_SPIEngineTaskStarted(void)
{
    ++bsp_wifi_spi_engine_task_start_count;
}

void BSP_WIFI_SPIEngineTaskWoke(uint32_t event_bits)
{
    bsp_wifi_spi_engine_last_event_bits = event_bits;
    ++bsp_wifi_spi_engine_task_wake_count;
}

void BSP_WIFI_SPIEngineDeinitialized(void)
{
    ++bsp_wifi_spi_engine_deinit_count;
}

uint32_t BSP_WIFI_GetSPIEngineTaskCreateStatus(void)
{
    return bsp_wifi_spi_engine_task_create_status;
}

uint32_t BSP_WIFI_GetSPIEngineTaskStartCount(void)
{
    return bsp_wifi_spi_engine_task_start_count;
}

uint32_t BSP_WIFI_GetSPIEngineTaskWakeCount(void)
{
    return bsp_wifi_spi_engine_task_wake_count;
}

uint32_t BSP_WIFI_GetSPIEngineLastEventBits(void)
{
    return bsp_wifi_spi_engine_last_event_bits;
}

uint32_t BSP_WIFI_GetSPIEngineDeinitCount(void)
{
    return bsp_wifi_spi_engine_deinit_count;
}
