#ifndef APP_H
#define APP_H

#include <stdint.h>

#include "bsp_power.h"

/* Temporary hardware-isolation build: keep the LTDC color-gradient pattern
   on screen and do not start TouchGFX/RTOS. Set to 0 after the test. */
#define APP_LCD_COLOR_DIAGNOSTIC 0

typedef enum
{
    APP_INIT_NOT_STARTED = 0,
    APP_INIT_OK,
    APP_INIT_FLASH_BSP_ERROR,
    APP_INIT_FLASH_MEMORY_MAPPED_ERROR,
    APP_INIT_FLASH_CONTENT_ERROR,
    APP_INIT_LCD_BSP_ERROR,
    APP_INIT_LCD_CONTROLLER_ERROR,
    APP_INIT_LCD_FRAMEBUFFER_ERROR,
    APP_INIT_LCD_BACKLIGHT_ERROR,
    APP_INIT_LED_BSP_ERROR,
    APP_INIT_LED_FILL_ERROR,
    APP_INIT_LED_TRANSFER_ERROR
} APP_InitStatus_t;

typedef struct
{
    uint32_t magic;
    uint32_t poll_count;
    uint32_t init_status;
    uint32_t system_core_clock_hz;
    uint32_t ltdc_gcr;
    uint32_t ltdc_cpsr;
    uint32_t ltdc_cdsr;
    uint32_t ltdc_isr;
    uint32_t ltdc_ier;
    uint32_t ltdc_error_count;
    uint32_t ltdc_last_error;
    uint32_t line_change_count;
    uint32_t layer_cfb_address;
    uint32_t framebuffer_first_word;
    uint32_t qspi_probe_word;
    uint32_t gpioa_idr;
    uint32_t gpioc_idr;
    uint32_t gpioe_idr;
    uint32_t gpiog_idr;
    uint32_t lcd_serial_clock_edges;
    uint32_t lcd_command_count;
} APP_DisplayDiagnostics_t;

extern volatile APP_DisplayDiagnostics_t app_display_diagnostics;

typedef struct
{
    uint32_t magic;
    uint32_t update_count;
    uint32_t error_count;
    BSP_POWER_Status_t status;
    BSP_POWER_Data_t data;
} APP_PowerDiagnostics_t;

extern volatile APP_PowerDiagnostics_t app_power_diagnostics;

void APP_Init(void);
void APP_LEDTask(void);
void APP_PowerTask(void);
APP_InitStatus_t APP_GetInitStatus(void);
void APP_DisplayDiagnosticsPoll(void);

#endif
