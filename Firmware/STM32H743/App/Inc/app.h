#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp_power.h"

typedef enum
{
    APP_INIT_NOT_STARTED = 0,
    APP_INIT_OK,
    APP_INIT_LCD_BSP_ERROR,
    APP_INIT_LCD_CONTROLLER_ERROR,
    APP_INIT_LCD_FRAMEBUFFER_ERROR,
    APP_INIT_LCD_BACKLIGHT_ERROR,
    APP_INIT_WAITING_FOR_TOF
} APP_InitStatus_t;

/* Task bodies, called from the CubeMX-generated wrappers in freertos.c. */
void APP_SystemTask(void);
void APP_LEDTask(void);
void APP_PowerTask(void);
void APP_SensorTask(void);
void APP_AudioOutputTask(void);
void APP_AudioInputTask(void);
void APP_WiFiTask(void);

void APP_Init(void);
APP_InitStatus_t APP_GetInitStatus(void);
/* UI-task request after the splash finishes; release occurs after rendering. */
void APP_DisplayStartupComplete(void);
void APP_DisplayFramePresented(void);

/* Normalized Hero breath (0..255), published by the UI task. */
void APP_LED_SetHeroBreath(uint8_t level);
/* One turn = 65536 phase units; visibility 0 restores the idle breath. */
void APP_LED_SetCarousel(uint16_t phase, uint8_t visibility);
/* UI-task entry confirmation; one brief brightness dip, never fully off. */
void APP_LED_MenuEnterPulse(void);
/* Hue of the idle breath (after startup). The LED task crossfades to it. */
void APP_LED_SetBreathColor(uint8_t red, uint8_t green, uint8_t blue);

/* Latest battery/charger sample. Returns false until the first valid read. */
bool APP_Power_GetData(BSP_POWER_Data_t *data);

/* Display signal level: 0 = no IP (crossed icon), 1..3 = bars from RSSI.
 * Safe to call from any task (single byte read). */
uint8_t APP_WiFi_GetSignalLevel(void);

#endif
