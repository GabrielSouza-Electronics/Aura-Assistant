#ifndef BSP_IMU_H
#define BSP_IMU_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BSP_IMU_OK = 0,
    BSP_IMU_ERROR_INVALID_ARGUMENT,
    BSP_IMU_ERROR_NOT_FOUND,
    BSP_IMU_ERROR_COMMUNICATION,
    BSP_IMU_ERROR_RESET_TIMEOUT,
    BSP_IMU_ERROR_NOT_INITIALIZED
} BSP_IMU_Status_t;

typedef struct
{
    uint8_t i2c_address_7bit;
    uint8_t who_am_i;
    bool acceleration_data_ready;
    bool gyroscope_data_ready;
    bool temperature_data_ready;
    int16_t acceleration_raw[3];
    int16_t gyroscope_raw[3];
    int16_t temperature_raw;
    int32_t acceleration_mg[3];
    int32_t gyroscope_mdps[3];
    int32_t temperature_mdeg_c;
    bool int1_pin_high;
    bool int2_pin_high;
} BSP_IMU_Data_t;

BSP_IMU_Status_t BSP_IMU_Init(void);
BSP_IMU_Status_t BSP_IMU_Read(BSP_IMU_Data_t *data);
BSP_IMU_Status_t BSP_IMU_EnableSingleTap(void);
BSP_IMU_Status_t BSP_IMU_TakeSingleTap(bool *detected);

#endif
