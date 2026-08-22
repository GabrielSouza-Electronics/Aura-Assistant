#include "app.h"

#include "bsp_flash.h"
#include "bsp_lcd.h"
#include "bsp_led.h"
#include "ltdc.h"

#define APP_LCD_WIDTH             480U
#define APP_LCD_HEIGHT            480U
#define APP_LCD_FRAMEBUFFER_WORDS (APP_LCD_WIDTH * APP_LCD_HEIGHT)
#define APP_FLASH_PROBE_BYTES     4096U

extern uint8_t __touchgfx_framebuffer_start__;
extern uint8_t __touchgfx_framebuffer_end__;
extern uint8_t __external_flash_start__;
extern uint8_t __external_flash_end__;

static volatile APP_InitStatus_t app_init_status = APP_INIT_NOT_STARTED;
volatile APP_DisplayDiagnostics_t app_display_diagnostics = {
    .magic = 0x41555241U /* "AURA" */
};

void APP_DisplayDiagnosticsPoll(void)
{
    static uint32_t previous_cpsr;
    const uint32_t current_cpsr = LTDC->CPSR;

    ++app_display_diagnostics.poll_count;
    app_display_diagnostics.init_status = (uint32_t)app_init_status;
    app_display_diagnostics.system_core_clock_hz = SystemCoreClock;
    app_display_diagnostics.ltdc_gcr = LTDC->GCR;
    app_display_diagnostics.ltdc_cpsr = current_cpsr;
    app_display_diagnostics.ltdc_cdsr = LTDC->CDSR;
    app_display_diagnostics.ltdc_isr = LTDC->ISR;
    app_display_diagnostics.ltdc_ier = LTDC->IER;
    app_display_diagnostics.layer_cfb_address = LTDC_Layer1->CFBAR;
    app_display_diagnostics.framebuffer_first_word =
        *(const volatile uint32_t *)(const void *)&__touchgfx_framebuffer_start__;
    app_display_diagnostics.qspi_probe_word =
        *(const volatile uint32_t *)(uintptr_t)0x90000624UL;
    app_display_diagnostics.gpioa_idr = GPIOA->IDR;
    app_display_diagnostics.gpioc_idr = GPIOC->IDR;
    app_display_diagnostics.gpioe_idr = GPIOE->IDR;
    app_display_diagnostics.gpiog_idr = GPIOG->IDR;
    app_display_diagnostics.lcd_serial_clock_edges =
        BSP_LCD_GetSerialClockEdgeCount();
    app_display_diagnostics.lcd_command_count = BSP_LCD_GetCommandCount();
    if (current_cpsr != previous_cpsr)
    {
        ++app_display_diagnostics.line_change_count;
        previous_cpsr = current_cpsr;
    }
}

void HAL_LTDC_ErrorCallback(LTDC_HandleTypeDef *handle)
{
    ++app_display_diagnostics.ltdc_error_count;
    app_display_diagnostics.ltdc_last_error = handle->ErrorCode;
}

static bool APP_FLASH_ValidateMappedContent(void)
{
    const volatile uint8_t *start =
        (const volatile uint8_t *)(const void *)&__external_flash_start__;
    const uintptr_t start_address = (uintptr_t)&__external_flash_start__;
    const uintptr_t end_address = (uintptr_t)&__external_flash_end__;
    size_t probe_length;
    bool found_nonzero = false;
    bool found_non_ff = false;

    if ((end_address <= start_address) ||
        ((end_address - start_address) > (16U * 1024U * 1024U)))
    {
        return false;
    }

    probe_length = end_address - start_address;
    if (probe_length > APP_FLASH_PROBE_BYTES)
    {
        probe_length = APP_FLASH_PROBE_BYTES;
    }

    for (size_t index = 0U; index < probe_length; ++index)
    {
        const uint8_t value = start[index];
        found_nonzero = found_nonzero || (value != 0x00U);
        found_non_ff = found_non_ff || (value != 0xFFU);
    }

    return found_nonzero && found_non_ff;
}

static bool APP_LCD_ClearFramebuffer(void)
{
    volatile uint16_t *framebuffer =
        (volatile uint16_t *)(void *)&__touchgfx_framebuffer_start__;
    const uintptr_t framebuffer_start =
        (uintptr_t)&__touchgfx_framebuffer_start__;
    const uintptr_t framebuffer_end =
        (uintptr_t)&__touchgfx_framebuffer_end__;
    size_t index;

    if ((framebuffer_end < framebuffer_start) ||
        ((framebuffer_end - framebuffer_start) <
         (APP_LCD_FRAMEBUFFER_WORDS * sizeof(uint16_t))))
    {
        return false;
    }

    for (index = 0U; index < APP_LCD_FRAMEBUFFER_WORDS; ++index)
    {
        framebuffer[index] = 0x0000U;
    }

    if (HAL_LTDC_SetAddress(&hltdc, (uint32_t)framebuffer_start, 0U) != HAL_OK)
    {
        return false;
    }

    return true;
}

void APP_Init(void)
{
    BSP_FLASH_Status_t flash_status;
    BSP_LCD_Status_t lcd_status;
    BSP_LED_Status_t led_status;

    app_init_status = APP_INIT_FLASH_BSP_ERROR;
    flash_status = BSP_FLASH_Init();
    if (flash_status != BSP_FLASH_OK)
    {
        return;
    }

    app_init_status = APP_INIT_FLASH_MEMORY_MAPPED_ERROR;
    flash_status = BSP_FLASH_EnableMemoryMappedMode();
    if (flash_status != BSP_FLASH_OK)
    {
        return;
    }

    app_init_status = APP_INIT_FLASH_CONTENT_ERROR;
    if (!APP_FLASH_ValidateMappedContent())
    {
        return;
    }

    app_init_status = APP_INIT_LCD_BSP_ERROR;
    lcd_status = BSP_LCD_Init();
    if (lcd_status != BSP_LCD_OK)
    {
        return;
    }

    app_init_status = APP_INIT_LCD_CONTROLLER_ERROR;
    lcd_status = BSP_LCD_InitController();
    if (lcd_status != BSP_LCD_OK)
    {
        return;
    }

    app_init_status = APP_INIT_LCD_FRAMEBUFFER_ERROR;
    if (!APP_LCD_ClearFramebuffer())
    {
        return;
    }

    app_init_status = APP_INIT_LCD_BACKLIGHT_ERROR;
    lcd_status = BSP_LCD_SetBacklight(true);
    if (lcd_status != BSP_LCD_OK)
    {
        return;
    }

    app_init_status = APP_INIT_LED_BSP_ERROR;
    led_status = BSP_LED_Init();
    if (led_status != BSP_LED_OK)
    {
        return;
    }

    app_init_status = APP_INIT_LED_FILL_ERROR;
    led_status = BSP_LED_Fill(255U, 0U, 0U);
    if (led_status != BSP_LED_OK)
    {
        return;
    }

    app_init_status = APP_INIT_LED_TRANSFER_ERROR;
    led_status = BSP_LED_ShowBlocking(10U);
    if (led_status != BSP_LED_OK)
    {
        return;
    }

    app_init_status = APP_INIT_OK;
    APP_DisplayDiagnosticsPoll();
}

APP_InitStatus_t APP_GetInitStatus(void)
{
    return app_init_status;
}
