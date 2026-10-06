#ifndef APP_UI_AUDIO_H
#define APP_UI_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    APP_UI_AUDIO_NONE = 0,
    APP_UI_AUDIO_TICK,
    APP_UI_AUDIO_ENTER,
    APP_UI_AUDIO_EXIT,
    APP_UI_AUDIO_TASK_COMPLETE,
    APP_UI_AUDIO_TASK_REOPEN
} APP_UIAudioEvent_t;

/* Task context, nonblocking. Completion takes priority over pending navigation. */
void APP_UIAudio_Request(APP_UIAudioEvent_t event);
/* Called only by AudioOutputTask after startup playback. Never returns. */
void APP_UIAudio_Run(void);
#ifdef __cplusplus
}
#endif
#endif
