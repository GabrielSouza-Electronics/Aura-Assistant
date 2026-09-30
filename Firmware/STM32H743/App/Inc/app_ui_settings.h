#ifndef APP_UI_SETTINGS_H
#define APP_UI_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Texts shown as values in the GUI Settings menu. Order matches the menu
   rows; the Sound row shows the audio volume and is not a free text. */
typedef enum {
    APP_UI_SETTING_WIFI = 0,
    APP_UI_SETTING_BLUETOOTH,
    APP_UI_SETTING_BRIGHTNESS,
    APP_UI_SETTING_TEXT_COUNT,
    /* Not a text: shows/changes the speaker volume. */
    APP_UI_SETTING_SOUND = APP_UI_SETTING_TEXT_COUNT,
    APP_UI_SETTING_COUNT
} APP_UISetting_t;

/* Including the terminator. Longer texts are truncated here; the GUI may
   shorten further (with "..") to fit the row. */
#define APP_UI_SETTING_TEXT_SIZE 33U

/* Any task context, nonblocking. The GUI picks the change up on its next
   tick. Text is shown in upper case; unsupported characters show as '?'. */
void APP_UISettings_SetText(APP_UISetting_t item, const char *text);

/* GUI task. Copies the text only if it changed since *version (start with
   0) and updates *version. Returns true when out was written. */
bool APP_UISettings_ReadText(APP_UISetting_t item, uint32_t *version,
                             char *out, size_t size);

/* Current speaker volume, 0 (min) .. 10 (max). */
uint8_t APP_UISettings_GetVolume(void);

/* GUI task. A user gesture in the Settings menu: delta +1 = hand up
   (on / increase), -1 = hand down (off / decrease). Wi-Fi is read-only.
   Brightness changes by 10%, sound by 1. BLE requests are asynchronous. */
void APP_UISettings_Request(APP_UISetting_t item, int8_t delta);


#ifdef __cplusplus
}
#endif
#endif
