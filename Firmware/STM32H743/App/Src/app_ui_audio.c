#include "app_ui_audio.h"
#include "app.h"
#include "bsp_audio_out.h"
#include "ui_audio.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"

#define UI_AUDIO_REQUEST_FLAG (1UL << 2)
static APP_UIAudioEvent_t pending;
static osThreadId_t owner;

void APP_UIAudio_Request(APP_UIAudioEvent_t event)
{
    osThreadId_t thread;
    if (APP_AUDIO_ECHO_TEST || event < APP_UI_AUDIO_TICK || event > APP_UI_AUDIO_EXIT)
        return;
    taskENTER_CRITICAL();
    pending = event;
    thread = owner;
    taskEXIT_CRITICAL();
    if (thread != NULL)
        (void)osThreadFlagsSet(thread, UI_AUDIO_REQUEST_FLAG);
}

void APP_UIAudio_Run(void)
{
    taskENTER_CRITICAL();
    owner = osThreadGetId();
    taskEXIT_CRITICAL();
    for (;;)
    {
        APP_UIAudioEvent_t event;
        taskENTER_CRITICAL();
        event = pending;
        pending = APP_UI_AUDIO_NONE;
        taskEXIT_CRITICAL();
        if (event != APP_UI_AUDIO_NONE)
        {
            const int16_t *pcm = ui_nav_tick;
            size_t count = UI_NAV_TICK_COUNT;
            if (event == APP_UI_AUDIO_ENTER) { pcm = ui_menu_enter; count = UI_MENU_ENTER_COUNT; }
            if (event == APP_UI_AUDIO_EXIT) { pcm = ui_menu_exit; count = UI_MENU_EXIT_COUNT; }
            app_audio_out_diagnostics.last_status = BSP_AUDIO_OUT_PlayEffect48kMono(pcm, count);
            if (app_audio_out_diagnostics.last_status == BSP_AUDIO_OUT_OK)
                ++app_audio_out_diagnostics.play_count;
            else
                ++app_audio_out_diagnostics.error_count;
        }
        app_audio_out_diagnostics.busy = BSP_AUDIO_OUT_IsBusy();
        app_audio_out_diagnostics.last_hal_error = BSP_AUDIO_OUT_GetLastHALerror();
        /* Poll completion diagnostics; new requests wake immediately. */
        (void)osThreadFlagsWait(UI_AUDIO_REQUEST_FLAG, osFlagsWaitAny, 10U);
    }
}
