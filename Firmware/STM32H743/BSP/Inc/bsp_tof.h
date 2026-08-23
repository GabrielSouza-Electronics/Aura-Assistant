#ifndef BSP_TOF_H
#define BSP_TOF_H

#include <stdbool.h>
#include <stdint.h>

#define BSP_TOF_ZONE_COUNT 16U

typedef enum
{
    BSP_TOF_OK = 0,
    BSP_TOF_NO_NEW_DATA,
    BSP_TOF_ERROR_INVALID_ARGUMENT,
    BSP_TOF_ERROR_NOT_FOUND,
    BSP_TOF_ERROR_INITIALIZATION,
    BSP_TOF_ERROR_CONFIGURATION,
    BSP_TOF_ERROR_START,
    BSP_TOF_ERROR_COMMUNICATION,
    BSP_TOF_ERROR_NOT_INITIALIZED
} BSP_TOF_Status_t;

typedef struct
{
    uint8_t i2c_address_7bit;
    uint8_t stream_count;
    int8_t sensor_temperature_c;
    bool interrupt_pin_high;
    uint8_t targets_detected[BSP_TOF_ZONE_COUNT];
    int16_t distance_mm[BSP_TOF_ZONE_COUNT];
    uint8_t target_status[BSP_TOF_ZONE_COUNT];
} BSP_TOF_Data_t;

BSP_TOF_Status_t BSP_TOF_Init(void);
BSP_TOF_Status_t BSP_TOF_Read(BSP_TOF_Data_t *data);

#endif
