#ifndef BSP_POWER_H
#define BSP_POWER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BSP_POWER_OK = 0,
    BSP_POWER_ERROR_INVALID_ARGUMENT,
    BSP_POWER_ERROR_ADC_CALIBRATION,
    BSP_POWER_ERROR_ADC_START,
    BSP_POWER_ERROR_ADC_TIMEOUT,
    BSP_POWER_ERROR_ADC_STOP
} BSP_POWER_Status_t;

typedef struct
{
    uint32_t battery_adc_raw;
    uint32_t battery_adc_mv;
    uint32_t battery_mv;
    uint8_t battery_percent;
    bool charger_pin_high;
    bool usb_status_pin_high;
    bool charging;
    bool usb_connected;
} BSP_POWER_Data_t;

BSP_POWER_Status_t BSP_POWER_Init(void);
BSP_POWER_Status_t BSP_POWER_Read(BSP_POWER_Data_t *data);

#endif
