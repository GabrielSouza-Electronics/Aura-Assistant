#include "bsp_power.h"

#include "adc.h"
#include "main.h"

#define BSP_POWER_ADC_FULL_SCALE          65535U
#define BSP_POWER_ADC_REFERENCE_MV        3300U
#define BSP_POWER_BATTERY_R_TOP_KOHM      100U
#define BSP_POWER_BATTERY_R_BOTTOM_KOHM   240U
#define BSP_POWER_BATTERY_EMPTY_MV        3200U
#define BSP_POWER_BATTERY_FULL_MV         4200U
#define BSP_POWER_ADC_TIMEOUT_MS          10U

static bool bsp_power_initialized;

static uint8_t BSP_POWER_EstimateBatteryPercent(uint32_t battery_mv)
{
    if (battery_mv <= BSP_POWER_BATTERY_EMPTY_MV)
    {
        return 0U;
    }

    if (battery_mv >= BSP_POWER_BATTERY_FULL_MV)
    {
        return 100U;
    }

    return (uint8_t)(((battery_mv - BSP_POWER_BATTERY_EMPTY_MV) * 100U) /
                     (BSP_POWER_BATTERY_FULL_MV - BSP_POWER_BATTERY_EMPTY_MV));
}

BSP_POWER_Status_t BSP_POWER_Init(void)
{
    if (bsp_power_initialized)
    {
        return BSP_POWER_OK;
    }

    if (HAL_ADCEx_Calibration_Start(&hadc1,
                                    ADC_CALIB_OFFSET_LINEARITY,
                                    ADC_SINGLE_ENDED) != HAL_OK)
    {
        return BSP_POWER_ERROR_ADC_CALIBRATION;
    }

    bsp_power_initialized = true;
    return BSP_POWER_OK;
}

BSP_POWER_Status_t BSP_POWER_Read(BSP_POWER_Data_t *data)
{
    uint32_t adc_raw;

    if (data == NULL)
    {
        return BSP_POWER_ERROR_INVALID_ARGUMENT;
    }

    if (!bsp_power_initialized)
    {
        BSP_POWER_Status_t status = BSP_POWER_Init();
        if (status != BSP_POWER_OK)
        {
            return status;
        }
    }

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return BSP_POWER_ERROR_ADC_START;
    }

    if (HAL_ADC_PollForConversion(&hadc1, BSP_POWER_ADC_TIMEOUT_MS) != HAL_OK)
    {
        (void)HAL_ADC_Stop(&hadc1);
        return BSP_POWER_ERROR_ADC_TIMEOUT;
    }

    adc_raw = HAL_ADC_GetValue(&hadc1);

    if (HAL_ADC_Stop(&hadc1) != HAL_OK)
    {
        return BSP_POWER_ERROR_ADC_STOP;
    }

    data->battery_adc_raw = adc_raw;
    data->battery_adc_mv =
        (adc_raw * BSP_POWER_ADC_REFERENCE_MV + (BSP_POWER_ADC_FULL_SCALE / 2U)) /
        BSP_POWER_ADC_FULL_SCALE;
    data->battery_mv =
        (data->battery_adc_mv *
         (BSP_POWER_BATTERY_R_TOP_KOHM + BSP_POWER_BATTERY_R_BOTTOM_KOHM) +
         (BSP_POWER_BATTERY_R_BOTTOM_KOHM / 2U)) /
        BSP_POWER_BATTERY_R_BOTTOM_KOHM;
    data->battery_percent = BSP_POWER_EstimateBatteryPercent(data->battery_mv);
    data->charger_pin_high =
        (HAL_GPIO_ReadPin(CHARGER_GPIO_Port, CHARGER_Pin) == GPIO_PIN_SET);
    data->usb_status_pin_high =
        (HAL_GPIO_ReadPin(USB_STATUS_GPIO_Port, USB_STATUS_Pin) == GPIO_PIN_SET);

    /* BQ24075 CHG and PGOOD are open-drain, active-low outputs. */
    data->charging = !data->charger_pin_high;
    data->usb_connected = !data->usb_status_pin_high;

    return BSP_POWER_OK;
}
