#include "app_ui_settings.h"

#include "FreeRTOS.h"
#include "task.h"
#include "bsp_audio_out.h"
#include "bsp_lcd.h"
#include "app_provisioning.h"
#include <stdio.h>
#include <string.h>

/* Initial states match boot: BLE closed, Wi-Fi disconnected, backlight at
   full duty. Owners publish changes. Version 1 forces the first GUI read. */
static struct
{
    char text[APP_UI_SETTING_TEXT_SIZE];
    uint32_t version;
} app_ui_settings[APP_UI_SETTING_TEXT_COUNT] = {
    { "DISCONNECTED", 1U }, { "OFF", 1U }, { "100%", 1U }
};

void APP_UISettings_SetText(APP_UISetting_t item, const char *text)
{
    if ((unsigned)item >= (unsigned)APP_UI_SETTING_TEXT_COUNT || text == NULL)
    {
        return;
    }
    /* Copy outside the critical section; publish with a short one. */
    char copy[APP_UI_SETTING_TEXT_SIZE];
    size_t n = 0U;
    while ((n < APP_UI_SETTING_TEXT_SIZE - 1U) && (text[n] != '\0'))
    {
        copy[n] = text[n];
        ++n;
    }
    copy[n] = '\0';

    taskENTER_CRITICAL();
    if (strcmp(app_ui_settings[item].text, copy) == 0)
    { taskEXIT_CRITICAL(); return; }
    for (size_t i = 0U; i <= n; ++i)
    {
        app_ui_settings[item].text[i] = copy[i];
    }
    ++app_ui_settings[item].version;
    taskEXIT_CRITICAL();
}

bool APP_UISettings_ReadText(APP_UISetting_t item, uint32_t *version,
                             char *out, size_t size)
{
    if ((unsigned)item >= (unsigned)APP_UI_SETTING_TEXT_COUNT ||
        version == NULL || out == NULL || size == 0U)
    {
        return false;
    }
    bool changed = false;
    taskENTER_CRITICAL();
    if (app_ui_settings[item].version != *version)
    {
        size_t i = 0U;
        while ((i < size - 1U) && (app_ui_settings[item].text[i] != '\0'))
        {
            out[i] = app_ui_settings[item].text[i];
            ++i;
        }
        out[i] = '\0';
        *version = app_ui_settings[item].version;
        changed = true;
    }
    taskEXIT_CRITICAL();
    return changed;
}

uint8_t APP_UISettings_GetVolume(void)
{
    return BSP_AUDIO_OUT_GetVolume();
}

void APP_UISettings_Request(APP_UISetting_t item, int8_t delta)
{
    if (delta == 0) { return; }
    const int32_t step = delta > 0 ? 1 : -1;
    if (item == APP_UI_SETTING_SOUND)
    {
        /* SetVolume clamps the top; clamp the bottom before the cast. */
        int32_t volume = (int32_t)BSP_AUDIO_OUT_GetVolume() + step;
        if (volume < 0)
        {
            volume = 0;
        }
        BSP_AUDIO_OUT_SetVolume((uint8_t)volume);
        return;
    }
    if (item == APP_UI_SETTING_BRIGHTNESS)
    {
        int32_t brightness = (int32_t)BSP_LCD_GetBrightness() + step * 10;
        if (brightness < 10) { brightness = 10; }
        if (brightness > 100) { brightness = 100; }
        if (BSP_LCD_SetBrightness((uint8_t)brightness) == BSP_LCD_OK)
        {
            char text[5];
            (void)snprintf(text, sizeof(text), "%u%%", (unsigned)brightness);
            APP_UISettings_SetText(item, text);
        }
    }
    else if (item == APP_UI_SETTING_BLUETOOTH)
    { APP_ProvisionRequestEnabled(step > 0); }
}
