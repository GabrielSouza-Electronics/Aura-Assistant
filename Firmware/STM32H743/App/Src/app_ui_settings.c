#include "app_ui_settings.h"

#include "FreeRTOS.h"
#include "task.h"
#include "bsp_audio_out.h"

/* Until the owning module publishes a real value, the menu shows "--"
   rather than a guessed state. Version starts at 1 so the GUI picks the
   placeholder up on its first read. */
static struct
{
    char text[APP_UI_SETTING_TEXT_SIZE];
    uint32_t version;
} app_ui_settings[APP_UI_SETTING_TEXT_COUNT] = {
    { "--", 1U }, { "--", 1U }, { "--", 1U }
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
    if (item == APP_UI_SETTING_SOUND)
    {
        /* SetVolume clamps the top; clamp the bottom before the cast. */
        int32_t volume = (int32_t)BSP_AUDIO_OUT_GetVolume() + delta;
        if (volume < 0)
        {
            volume = 0;
        }
        BSP_AUDIO_OUT_SetVolume((uint8_t)volume);
        return;
    }
    if ((unsigned)item < (unsigned)APP_UI_SETTING_TEXT_COUNT)
    {
        APP_UISettings_OnRequest(item, delta);
    }
}

__attribute__((weak)) void APP_UISettings_OnRequest(APP_UISetting_t item,
                                                    int8_t delta)
{
    (void)item;
    (void)delta;
}
