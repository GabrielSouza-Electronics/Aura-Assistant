#ifndef AUDIO_SFX_H
#define AUDIO_SFX_H

#include <stddef.h>
#include <stdint.h>

#define AUDIO_SFX_SAMPLE_RATE_HZ 48000U

typedef enum
{
    AUDIO_SFX_WAKE = 0,
    AUDIO_SFX_MENU_CHANGE,
    AUDIO_SFX_OPTION_CHANGE,
    AUDIO_SFX_CONFIRM,
    AUDIO_SFX_SLEEP,
    AUDIO_SFX_ERROR,
    AUDIO_SFX_COUNT
} AUDIO_SFX_Id_t;

size_t AUDIO_SFX_RenderStereo(AUDIO_SFX_Id_t effect,
                              int16_t *output,
                              size_t stereo_sample_capacity);
const int16_t *AUDIO_SFX_GetStartupVoice(size_t *sample_count);

#endif
