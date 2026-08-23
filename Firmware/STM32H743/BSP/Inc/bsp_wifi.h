#ifndef BSP_WIFI_H
#define BSP_WIFI_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BSP_WIFI_OK = 0,
    BSP_WIFI_ERROR_INVALID_STATE,
    BSP_WIFI_ERROR_SPI_INIT
} BSP_WIFI_Status_t;

BSP_WIFI_Status_t BSP_WIFI_Init(void);
void BSP_WIFI_DeInit(void);
bool BSP_WIFI_IsPowered(void);
bool BSP_WIFI_IsEnabled(void);
bool BSP_WIFI_IsReady(void);
void BSP_WIFI_ReadyIRQObserved(void);
uint32_t BSP_WIFI_GetReadyIRQCount(void);
uint32_t BSP_WIFI_GetSPIStateBeforeInit(void);
uint32_t BSP_WIFI_GetSPIStateAfterInit(void);
uint32_t BSP_WIFI_GetSPIError(void);
uint32_t BSP_WIFI_GetSPIReinitCount(void);
void BSP_WIFI_SPITransferObserved(uint32_t hal_status,
                                  const void *rx_data,
                                  uint32_t length);
void BSP_WIFI_CSObserved(bool asserted);
uint32_t BSP_WIFI_GetSPITransferCount(void);
uint32_t BSP_WIFI_GetSPILastHALStatus(void);
uint32_t BSP_WIFI_GetSPILastLength(void);
uint32_t BSP_WIFI_GetCSAssertCount(void);
uint32_t BSP_WIFI_GetCSDeassertCount(void);
void BSP_WIFI_GetSPILastRX(uint8_t destination[8]);
void BSP_WIFI_SPIEngineTaskCreated(uint32_t create_status);
void BSP_WIFI_SPIEngineTaskStarted(void);
void BSP_WIFI_SPIEngineTaskWoke(uint32_t event_bits);
void BSP_WIFI_SPIEngineDeinitialized(void);
uint32_t BSP_WIFI_GetSPIEngineTaskCreateStatus(void);
uint32_t BSP_WIFI_GetSPIEngineTaskStartCount(void);
uint32_t BSP_WIFI_GetSPIEngineTaskWakeCount(void);
uint32_t BSP_WIFI_GetSPIEngineLastEventBits(void);
uint32_t BSP_WIFI_GetSPIEngineDeinitCount(void);

#endif
