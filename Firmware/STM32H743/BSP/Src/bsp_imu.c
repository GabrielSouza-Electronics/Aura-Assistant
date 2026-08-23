#include "bsp_imu.h"

#include "i2c.h"
#include "ism330dlc_reg.h"
#include "main.h"

#define BSP_IMU_ADDRESS_LOW_7BIT    0x6AU
#define BSP_IMU_ADDRESS_HIGH_7BIT   0x6BU
#define BSP_IMU_I2C_TIMEOUT_MS      100U
#define BSP_IMU_RESET_TIMEOUT_MS    100U

static stmdev_ctx_t bsp_imu_context;
static uint16_t bsp_imu_address_hal;
static uint8_t bsp_imu_who_am_i;
static bool bsp_imu_initialized;

static int32_t BSP_IMU_Write(void *handle, uint8_t reg,
                             const uint8_t *buffer, uint16_t length)
{
    const uint16_t address = *(const uint16_t *)handle;
    return (HAL_I2C_Mem_Write(&hi2c4, address, reg, I2C_MEMADD_SIZE_8BIT,
                              (uint8_t *)(uintptr_t)buffer, length,
                              BSP_IMU_I2C_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
}

static int32_t BSP_IMU_ReadRegisters(void *handle, uint8_t reg,
                                     uint8_t *buffer, uint16_t length)
{
    const uint16_t address = *(const uint16_t *)handle;
    return (HAL_I2C_Mem_Read(&hi2c4, address, reg, I2C_MEMADD_SIZE_8BIT,
                             buffer, length, BSP_IMU_I2C_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
}

static void BSP_IMU_Delay(uint32_t milliseconds)
{
    HAL_Delay(milliseconds);
}

static bool BSP_IMU_TryAddress(uint8_t address_7bit)
{
    bsp_imu_address_hal = (uint16_t)address_7bit << 1;
    bsp_imu_who_am_i = 0U;
    return (ism330dlc_device_id_get(&bsp_imu_context, &bsp_imu_who_am_i) == 0) &&
           (bsp_imu_who_am_i == ISM330DLC_ID);
}

BSP_IMU_Status_t BSP_IMU_Init(void)
{
    uint8_t reset;
    uint32_t start_tick;

    bsp_imu_context.write_reg = BSP_IMU_Write;
    bsp_imu_context.read_reg = BSP_IMU_ReadRegisters;
    bsp_imu_context.mdelay = BSP_IMU_Delay;
    bsp_imu_context.handle = &bsp_imu_address_hal;
    bsp_imu_context.priv_data = NULL;

    HAL_Delay(15U);
    if (!BSP_IMU_TryAddress(BSP_IMU_ADDRESS_LOW_7BIT) &&
        !BSP_IMU_TryAddress(BSP_IMU_ADDRESS_HIGH_7BIT))
    {
        return BSP_IMU_ERROR_NOT_FOUND;
    }

    if (ism330dlc_reset_set(&bsp_imu_context, PROPERTY_ENABLE) != 0)
    {
        return BSP_IMU_ERROR_COMMUNICATION;
    }

    start_tick = HAL_GetTick();
    do
    {
        if (ism330dlc_reset_get(&bsp_imu_context, &reset) != 0)
        {
            return BSP_IMU_ERROR_COMMUNICATION;
        }
        if ((HAL_GetTick() - start_tick) > BSP_IMU_RESET_TIMEOUT_MS)
        {
            return BSP_IMU_ERROR_RESET_TIMEOUT;
        }
    } while (reset != 0U);

    if ((ism330dlc_block_data_update_set(&bsp_imu_context, PROPERTY_ENABLE) != 0) ||
        (ism330dlc_xl_full_scale_set(&bsp_imu_context, ISM330DLC_2g) != 0) ||
        (ism330dlc_gy_full_scale_set(&bsp_imu_context, ISM330DLC_250dps) != 0) ||
        (ism330dlc_xl_data_rate_set(&bsp_imu_context, ISM330DLC_XL_ODR_52Hz) != 0) ||
        (ism330dlc_gy_data_rate_set(&bsp_imu_context, ISM330DLC_GY_ODR_52Hz) != 0))
    {
        return BSP_IMU_ERROR_COMMUNICATION;
    }

    bsp_imu_initialized = true;
    return BSP_IMU_OK;
}

BSP_IMU_Status_t BSP_IMU_Read(BSP_IMU_Data_t *data)
{
    ism330dlc_status_reg_t ready = {0};

    if (data == NULL)
    {
        return BSP_IMU_ERROR_INVALID_ARGUMENT;
    }
    if (!bsp_imu_initialized)
    {
        return BSP_IMU_ERROR_NOT_INITIALIZED;
    }
    if (ism330dlc_status_reg_get(&bsp_imu_context, &ready) != 0)
    {
        return BSP_IMU_ERROR_COMMUNICATION;
    }

    data->i2c_address_7bit = (uint8_t)(bsp_imu_address_hal >> 1);
    data->who_am_i = bsp_imu_who_am_i;
    data->acceleration_data_ready = (ready.xlda != 0U);
    data->gyroscope_data_ready = (ready.gda != 0U);
    data->temperature_data_ready = (ready.tda != 0U);

    if (data->acceleration_data_ready &&
        (ism330dlc_acceleration_raw_get(&bsp_imu_context, data->acceleration_raw) != 0))
    {
        return BSP_IMU_ERROR_COMMUNICATION;
    }
    if (data->gyroscope_data_ready &&
        (ism330dlc_angular_rate_raw_get(&bsp_imu_context, data->gyroscope_raw) != 0))
    {
        return BSP_IMU_ERROR_COMMUNICATION;
    }
    if (data->temperature_data_ready &&
        (ism330dlc_temperature_raw_get(&bsp_imu_context, &data->temperature_raw) != 0))
    {
        return BSP_IMU_ERROR_COMMUNICATION;
    }

    for (uint32_t axis = 0U; axis < 3U; ++axis)
    {
        data->acceleration_mg[axis] =
            (int32_t)ism330dlc_from_fs2g_to_mg(data->acceleration_raw[axis]);
        data->gyroscope_mdps[axis] =
            (int32_t)ism330dlc_from_fs250dps_to_mdps(data->gyroscope_raw[axis]);
    }
    data->temperature_mdeg_c =
        (int32_t)(ism330dlc_from_lsb_to_celsius(data->temperature_raw) * 1000.0f);
    data->int1_pin_high =
        (HAL_GPIO_ReadPin(IMU_INT1_GPIO_Port, IMU_INT1_Pin) == GPIO_PIN_SET);
    data->int2_pin_high =
        (HAL_GPIO_ReadPin(IMU_INT2_GPIO_Port, IMU_INT2_Pin) == GPIO_PIN_SET);

    return BSP_IMU_OK;
}
