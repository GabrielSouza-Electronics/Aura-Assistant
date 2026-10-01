#include "app_ui_audio.h"
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
    if (event < APP_UI_AUDIO_TICK || event > APP_UI_AUDIO_EXIT)
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
            /* A failed effect is dropped; the next UI event retries. */
            (void)BSP_AUDIO_OUT_PlayEffect48kMono(pcm, count);
        }
        /* Requests set the flag after publishing `pending`, so none is lost. */
        (void)osThreadFlagsWait(UI_AUDIO_REQUEST_FLAG, osFlagsWaitAny,
                                osWaitForever);
    }
}
