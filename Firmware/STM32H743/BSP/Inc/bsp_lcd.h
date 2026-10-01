#ifndef BSP_LCD_H
#define BSP_LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    BSP_LCD_OK = 0,
    BSP_LCD_ERROR_NOT_INITIALIZED,
    BSP_LCD_ERROR_INVALID_ARGUMENT,
    BSP_LCD_ERROR_COMPONENT
} BSP_LCD_Status_t;

/**
 * Register the Aura LCD GPIO transport and place the panel in a safe state.
 *
 * This function does not release reset, run a controller register profile or
 * enable the backlight.
 */
BSP_LCD_Status_t BSP_LCD_Init(void);

/** Apply the hardware reset timing specified by the LCD module datasheet. */
BSP_LCD_Status_t BSP_LCD_Reset(void);

/** Control the active-high LCD backlight enable signal. */
BSP_LCD_Status_t BSP_LCD_SetBacklight(bool enabled);
/* Brightness duty cycle: 10..100%, in steps of 10. Retained while off. */
BSP_LCD_Status_t BSP_LCD_SetBrightness(uint8_t percent);
uint8_t BSP_LCD_GetBrightness(void);
/* Called only from the existing 1 ms HAL timebase ISR; no RTOS calls. */
void BSP_LCD_BacklightTick1ms(void);

/** Send one ST7701S command through the Aura 3-line serial GPIO interface. */
BSP_LCD_Status_t BSP_LCD_WriteCommand(uint8_t command,
                                      const uint8_t *parameters,
                                      size_t parameter_count);

/** Run the DWIN-provided ST7701S profile for the LI48480T028BA3098. */
BSP_LCD_Status_t BSP_LCD_InitController(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LCD_H */
