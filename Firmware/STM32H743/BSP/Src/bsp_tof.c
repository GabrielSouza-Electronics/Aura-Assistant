#include "bsp_tof.h"

#include "i2c.h"
#include "main.h"
#include "vl53l5cx_api.h"

#define BSP_TOF_I2C_ADDRESS_HAL      0x52U
#define BSP_TOF_I2C_ADDRESS_7BIT     0x29U
/* The official ULD uploads firmware in 32 KiB transactions. At the current
   100 kHz I2C2 rate, one such transaction takes about 3 seconds. */
#define BSP_TOF_I2C_TIMEOUT_MS       5000U
#define BSP_TOF_RANGING_FREQUENCY_HZ 5U

static VL53L5CX_Configuration bsp_tof_device;
static VL53L5CX_ResultsData bsp_tof_results;
static bool bsp_tof_initialized;

static int32_t BSP_TOF_Write(uint16_t address, uint16_t reg,
                             uint8_t *buffer, uint16_t length)
{
    return (HAL_I2C_Mem_Write(&hi2c2, address, reg, I2C_MEMADD_SIZE_16BIT,
                              buffer, length, BSP_TOF_I2C_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
}

static int32_t BSP_TOF_ReadRegisters(uint16_t address, uint16_t reg,
                                     uint8_t *buffer, uint16_t length)
{
    return (HAL_I2C_Mem_Read(&hi2c2, address, reg, I2C_MEMADD_SIZE_16BIT,
                             buffer, length, BSP_TOF_I2C_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
}

static int32_t BSP_TOF_GetTick(void)
{
    return (int32_t)HAL_GetTick();
}

BSP_TOF_Status_t BSP_TOF_Init(void)
{
    uint8_t alive = 0U;

    HAL_GPIO_WritePin(TOF_LPn_GPIO_Port, TOF_LPn_Pin, GPIO_PIN_RESET);
    HAL_Delay(10U);
    HAL_GPIO_WritePin(TOF_LPn_GPIO_Port, TOF_LPn_Pin, GPIO_PIN_SET);
    HAL_Delay(10U);

    bsp_tof_device.platform.address = BSP_TOF_I2C_ADDRESS_HAL;
    bsp_tof_device.platform.Write = BSP_TOF_Write;
    bsp_tof_device.platform.Read = BSP_TOF_ReadRegisters;
    bsp_tof_device.platform.GetTick = BSP_TOF_GetTick;

    if ((vl53l5cx_is_alive(&bsp_tof_device, &alive) != 0U) || (alive == 0U))
    {
        return BSP_TOF_ERROR_NOT_FOUND;
    }
    if (vl53l5cx_init(&bsp_tof_device) != 0U)
    {
        return BSP_TOF_ERROR_INITIALIZATION;
    }
    if ((vl53l5cx_set_resolution(&bsp_tof_device, VL53L5CX_RESOLUTION_4X4) != 0U) ||
        (vl53l5cx_set_ranging_frequency_hz(&bsp_tof_device,
                                           BSP_TOF_RANGING_FREQUENCY_HZ) != 0U))
    {
        return BSP_TOF_ERROR_CONFIGURATION;
    }
    if (vl53l5cx_start_ranging(&bsp_tof_device) != 0U)
    {
        return BSP_TOF_ERROR_START;
    }

    bsp_tof_initialized = true;
    return BSP_TOF_OK;
}

BSP_TOF_Status_t BSP_TOF_Read(BSP_TOF_Data_t *data)
{
    uint8_t ready = 0U;

    if (data == NULL)
    {
        return BSP_TOF_ERROR_INVALID_ARGUMENT;
    }
    if (!bsp_tof_initialized)
    {
        return BSP_TOF_ERROR_NOT_INITIALIZED;
    }
    if (vl53l5cx_check_data_ready(&bsp_tof_device, &ready) != 0U)
    {
        return BSP_TOF_ERROR_COMMUNICATION;
    }
    if (ready == 0U)
    {
        return BSP_TOF_NO_NEW_DATA;
    }
    if (vl53l5cx_get_ranging_data(&bsp_tof_device, &bsp_tof_results) != 0U)
    {
        return BSP_TOF_ERROR_COMMUNICATION;
    }

    data->i2c_address_7bit = BSP_TOF_I2C_ADDRESS_7BIT;
    data->stream_count = bsp_tof_device.streamcount;
    data->sensor_temperature_c = bsp_tof_results.silicon_temp_degc;
    data->interrupt_pin_high =
        (HAL_GPIO_ReadPin(TOF_INT_GPIO_Port, TOF_INT_Pin) == GPIO_PIN_SET);
    for (uint32_t zone = 0U; zone < BSP_TOF_ZONE_COUNT; ++zone)
    {
        data->targets_detected[zone] = bsp_tof_results.nb_target_detected[zone];
        data->distance_mm[zone] = bsp_tof_results.distance_mm[zone];
        data->target_status[zone] = bsp_tof_results.target_status[zone];
    }

    return BSP_TOF_OK;
}
