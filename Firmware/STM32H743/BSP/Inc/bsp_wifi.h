#ifndef BSP_WIFI_H
#define BSP_WIFI_H

#include <stdbool.h>

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

#endif
